// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_types.h"

#include <limits>

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/history/page_usage_data.h"

namespace history {

// VisitRow --------------------------------------------------------------------

VisitRow::VisitRow()
    : visit_id(0),
      url_id(0),
      referring_visit(0),
      transition(content::PAGE_TRANSITION_LINK),
      segment_id(0),
      is_indexed(false) {
}

VisitRow::VisitRow(URLID arg_url_id,
                   base::Time arg_visit_time,
                   VisitID arg_referring_visit,
                   content::PageTransition arg_transition,
                   SegmentID arg_segment_id)
    : visit_id(0),
      url_id(arg_url_id),
      visit_time(arg_visit_time),
      referring_visit(arg_referring_visit),
      transition(arg_transition),
      segment_id(arg_segment_id),
      is_indexed(false) {
}

VisitRow::~VisitRow() {
}

// URLResult -------------------------------------------------------------------

URLResult::URLResult() {
}

URLResult::URLResult(const GURL& url, base::Time visit_time)
    : URLRow(url),
      visit_time_(visit_time) {
}

URLResult::URLResult(const GURL& url,
                     const Snippet::MatchPositions& title_matches)
    : URLRow(url) {
  title_match_positions_ = title_matches;
}

URLResult::~URLResult() {
}

void URLResult::SwapResult(URLResult* other) {
  URLRow::Swap(other);
  std::swap(visit_time_, other->visit_time_);
  snippet_.Swap(&other->snippet_);
  title_match_positions_.swap(other->title_match_positions_);
}

// QueryResults ----------------------------------------------------------------

QueryResults::QueryResults() : reached_beginning_(false) {
}

QueryResults::~QueryResults() {
  // Free all the URL objects.
  STLDeleteContainerPointers(results_.begin(), results_.end());
}

const size_t* QueryResults::MatchesForURL(const GURL& url,
                                          size_t* num_matches) const {
  URLToResultIndices::const_iterator found = url_to_results_.find(url);
  if (found == url_to_results_.end()) {
    if (num_matches)
      *num_matches = 0;
    return NULL;
  }

  // All entries in the map should have at least one index, otherwise it
  // shouldn't be in the map.
  DCHECK(!found->second->empty());
  if (num_matches)
    *num_matches = found->second->size();
  return &found->second->front();
}

void QueryResults::Swap(QueryResults* other) {
  std::swap(first_time_searched_, other->first_time_searched_);
  std::swap(reached_beginning_, other->reached_beginning_);
  results_.swap(other->results_);
  url_to_results_.swap(other->url_to_results_);
}

void QueryResults::AppendURLBySwapping(URLResult* result) {
  URLResult* new_result = new URLResult;
  new_result->SwapResult(result);

  results_.push_back(new_result);
  AddURLUsageAtIndex(new_result->url(), results_.size() - 1);
}

void QueryResults::AppendResultsBySwapping(QueryResults* other,
                                           bool remove_dupes) {
  if (remove_dupes) {
    // Delete all entries in the other array that are already in this one.
    for (size_t i = 0; i < results_.size(); i++)
      other->DeleteURL(results_[i]->url());
  }

  if (first_time_searched_ > other->first_time_searched_)
    std::swap(first_time_searched_, other->first_time_searched_);

  if (reached_beginning_ != other->reached_beginning_)
    std::swap(reached_beginning_, other->reached_beginning_);

  for (size_t i = 0; i < other->results_.size(); i++) {
    // Just transfer pointer ownership.
    results_.push_back(other->results_[i]);
    AddURLUsageAtIndex(results_.back()->url(), results_.size() - 1);
  }

  // We just took ownership of all the results in the input vector.
  other->results_.clear();
  other->url_to_results_.clear();
}

void QueryResults::DeleteURL(const GURL& url) {
  // Delete all instances of this URL. We re-query each time since each
  // mutation will cause the indices to change.
  while (const size_t* match_indices = MatchesForURL(url, NULL))
    DeleteRange(*match_indices, *match_indices);
}

void QueryResults::DeleteRange(size_t begin, size_t end) {
  DCHECK(begin <= end && begin < size() && end < size());

  // First delete the pointers in the given range and store all the URLs that
  // were modified. We will delete references to these later.
  std::set<GURL> urls_modified;
  for (size_t i = begin; i <= end; i++) {
    urls_modified.insert(results_[i]->url());
    delete results_[i];
    results_[i] = NULL;
  }

  // Now just delete that range in the vector en masse (the STL ending is
  // exclusive, while ours is inclusive, hence the +1).
  results_.erase(results_.begin() + begin, results_.begin() + end + 1);

  // Delete the indicies referencing the deleted entries.
  for (std::set<GURL>::const_iterator url = urls_modified.begin();
       url != urls_modified.end(); ++url) {
    URLToResultIndices::iterator found = url_to_results_.find(*url);
    if (found == url_to_results_.end()) {
      NOTREACHED();
      continue;
    }

    // Need a signed loop type since we do -- which may take us to -1.
    for (int match = 0; match < static_cast<int>(found->second->size());
         match++) {
      if (found->second[match] >= begin && found->second[match] <= end) {
        // Remove this referece from the list.
        found->second->erase(found->second->begin() + match);
        match--;
      }
    }

    // Clear out an empty lists if we just made one.
    if (found->second->empty())
      url_to_results_.erase(found);
  }

  // Shift all other indices over to account for the removed ones.
  AdjustResultMap(end + 1, std::numeric_limits<size_t>::max(),
                  -static_cast<ptrdiff_t>(end - begin + 1));
}

void QueryResults::AddURLUsageAtIndex(const GURL& url, size_t index) {
  URLToResultIndices::iterator found = url_to_results_.find(url);
  if (found != url_to_results_.end()) {
    // The URL is already in the list, so we can just append the new index.
    found->second->push_back(index);
    return;
  }

  // Need to add a new entry for this URL.
  StackVector<size_t, 4> new_list;
  new_list->push_back(index);
  url_to_results_[url] = new_list;
}

void QueryResults::AdjustResultMap(size_t begin, size_t end, ptrdiff_t delta) {
  for (URLToResultIndices::iterator i = url_to_results_.begin();
       i != url_to_results_.end(); ++i) {
    for (size_t match = 0; match < i->second->size(); match++) {
      size_t match_index = i->second[match];
      if (match_index >= begin && match_index <= end)
        i->second[match] += delta;
    }
  }
}

// QueryOptions ----------------------------------------------------------------

QueryOptions::QueryOptions() : max_count(0), body_only(false) {}

void QueryOptions::SetRecentDayRange(int days_ago) {
  end_time = base::Time::Now();
  begin_time = end_time - base::TimeDelta::FromDays(days_ago);
}

// MostVisitedURL --------------------------------------------------------------

MostVisitedURL::MostVisitedURL() {}

MostVisitedURL::MostVisitedURL(const GURL& url,
                               const string16& title)
    : url(url),
      title(title) {
}

MostVisitedURL::~MostVisitedURL() {}

// FilteredURL -----------------------------------------------------------------

FilteredURL::FilteredURL() : score(0.0) {}

FilteredURL::FilteredURL(const PageUsageData& page_data)
    : url(page_data.GetURL()),
      title(page_data.GetTitle()),
      score(page_data.GetScore()) {
}

FilteredURL::~FilteredURL() {}

// FilteredURL::ExtendedInfo ---------------------------------------------------

FilteredURL::ExtendedInfo::ExtendedInfo()
    : total_visits(0),
      visits(0),
      duration_opened(0) {
}

// Images ---------------------------------------------------------------------

Images::Images() {}

Images::~Images() {}

// TopSitesDelta --------------------------------------------------------------

TopSitesDelta::TopSitesDelta() {}

TopSitesDelta::~TopSitesDelta() {}

// HistoryAddPageArgs ---------------------------------------------------------

HistoryAddPageArgs::HistoryAddPageArgs()
    : id_scope(NULL),
      page_id(0),
      transition(content::PAGE_TRANSITION_LINK),
      visit_source(SOURCE_BROWSED),
      did_replace_entry(false) {}

HistoryAddPageArgs::HistoryAddPageArgs(
    const GURL& url,
    base::Time time,
    const void* id_scope,
    int32 page_id,
    const GURL& referrer,
    const history::RedirectList& redirects,
    content::PageTransition transition,
    VisitSource source,
    bool did_replace_entry)
      : url(url),
        time(time),
        id_scope(id_scope),
        page_id(page_id),
        referrer(referrer),
        redirects(redirects),
        transition(transition),
        visit_source(source),
        did_replace_entry(did_replace_entry) {
}

HistoryAddPageArgs::~HistoryAddPageArgs() {}

ThumbnailMigration::ThumbnailMigration() {}

ThumbnailMigration::~ThumbnailMigration() {}

MostVisitedThumbnails::MostVisitedThumbnails() {}

MostVisitedThumbnails::~MostVisitedThumbnails() {}

// FaviconBitmapResult --------------------------------------------------------

FaviconBitmapResult::FaviconBitmapResult()
    : expired(false),
      icon_type(history::INVALID_ICON) {
}

FaviconBitmapResult::~FaviconBitmapResult() {
}

// FaviconImageResult ---------------------------------------------------------

FaviconImageResult::FaviconImageResult() {
}

FaviconImageResult::~FaviconImageResult() {
}

// FaviconSizes --------------------------------------------------------------

const FaviconSizes& GetDefaultFaviconSizes() {
  CR_DEFINE_STATIC_LOCAL(FaviconSizes, kDefaultFaviconSizes, ());
  return kDefaultFaviconSizes;
}

// FaviconBitmapIDSize ---------------------------------------------------------

FaviconBitmapIDSize::FaviconBitmapIDSize()
    : bitmap_id(0) {
}

FaviconBitmapIDSize::~FaviconBitmapIDSize() {
}

// FaviconBitmap --------------------------------------------------------------

FaviconBitmap::FaviconBitmap()
    : bitmap_id(0),
      icon_id(0) {
}

FaviconBitmap::~FaviconBitmap() {
}

// ImportedFaviconUsage --------------------------------------------------------

ImportedFaviconUsage::ImportedFaviconUsage() {
}

ImportedFaviconUsage::~ImportedFaviconUsage() {
}

}  // namespace history
