// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCSchedulerStateMachine.h"

#include <gtest/gtest.h>

using namespace cc;
using namespace WTF;

namespace {

const CCSchedulerStateMachine::CommitState allCommitStates[] = {
    CCSchedulerStateMachine::COMMIT_STATE_IDLE,
    CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS,
    CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES,
    CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT,
    CCSchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW
};

// Exposes the protected state fields of the CCSchedulerStateMachine for testing
class StateMachine : public CCSchedulerStateMachine {
public:
    void setCommitState(CommitState cs) { m_commitState = cs; }
    CommitState commitState() const { return  m_commitState; }

    void setNeedsCommit(bool b) { m_needsCommit = b; }
    bool needsCommit() const { return m_needsCommit; }

    void setNeedsForcedCommit(bool b) { m_needsForcedCommit = b; }
    bool needsForcedCommit() const { return m_needsForcedCommit; }

    void setNeedsRedraw(bool b) { m_needsRedraw = b; }
    bool needsRedraw() const { return m_needsRedraw; }

    void setNeedsForcedRedraw(bool b) { m_needsForcedRedraw = b; }
    bool needsForcedRedraw() const { return m_needsForcedRedraw; }

    bool canDraw() const { return m_canDraw; }
    bool insideVSync() const { return m_insideVSync; }
    bool visible() const { return m_visible; }

    void setUpdateResourcesCompletePending(bool b) { m_updateResourcesCompletePending = b; }
    bool updateResourcesCompletePending() const { return m_updateResourcesCompletePending; }
};

TEST(CCSchedulerStateMachineTest, TestNextActionBeginsFrameIfNeeded)
{
    // If no commit needed, do nothing
    {
        StateMachine state;
        state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_IDLE);
        state.setCanBeginFrame(true);
        state.setNeedsRedraw(false);
        state.setNeedsCommit(false);
        state.setUpdateResourcesCompletePending(false);
        state.setVisible(true);

        EXPECT_FALSE(state.vsyncCallbackNeeded());

        state.didLeaveVSync();
        EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
        EXPECT_FALSE(state.vsyncCallbackNeeded());
        state.didEnterVSync();
        EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    }

    // If commit requested but canBeginFrame is still false, do nothing.
    {
        StateMachine state;
        state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_IDLE);
        state.setNeedsRedraw(false);
        state.setNeedsCommit(false);
        state.setUpdateResourcesCompletePending(false);
        state.setVisible(true);

        EXPECT_FALSE(state.vsyncCallbackNeeded());

        state.didLeaveVSync();
        EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
        EXPECT_FALSE(state.vsyncCallbackNeeded());
        state.didEnterVSync();
        EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    }


    // If commit requested, begin a frame
    {
        StateMachine state;
        state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_IDLE);
        state.setCanBeginFrame(true);
        state.setNeedsRedraw(false);
        state.setNeedsCommit(true);
        state.setUpdateResourcesCompletePending(false);
        state.setVisible(true);
        EXPECT_FALSE(state.vsyncCallbackNeeded());
    }

    // Begin the frame, make sure needsCommit and commitState update correctly.
    {
        StateMachine state;
        state.setCanBeginFrame(true);
        state.setVisible(true);
        state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_FRAME);
        EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
        EXPECT_FALSE(state.needsCommit());
        EXPECT_FALSE(state.vsyncCallbackNeeded());
    }
}

TEST(CCSchedulerStateMachineTest, TestSetForcedRedrawDoesNotSetsNormalRedraw)
{
    CCSchedulerStateMachine state;
    state.setCanDraw(true);
    state.setNeedsForcedRedraw();
    EXPECT_FALSE(state.redrawPending());
    EXPECT_TRUE(state.vsyncCallbackNeeded());
}

TEST(CCSchedulerStateMachineTest, TestFailedDrawSetsNeedsCommitAndDoesNotDrawAgain)
{
    CCSchedulerStateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);
    state.setNeedsRedraw();
    EXPECT_TRUE(state.redrawPending());
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();

    // We're drawing now.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    EXPECT_FALSE(state.redrawPending());
    EXPECT_FALSE(state.commitPending());

    // Failing the draw makes us require a commit.
    state.didDrawIfPossibleCompleted(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_TRUE(state.redrawPending());
    EXPECT_TRUE(state.commitPending());
}

TEST(CCSchedulerStateMachineTest, TestSetNeedsRedrawDuringFailedDrawDoesNotRemoveNeedsRedraw)
{
    CCSchedulerStateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);
    state.setNeedsRedraw();
    EXPECT_TRUE(state.redrawPending());
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();

    // We're drawing now.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    EXPECT_FALSE(state.redrawPending());
    EXPECT_FALSE(state.commitPending());

    // While still in the same vsync callback, set needs redraw again.
    // This should not redraw.
    state.setNeedsRedraw();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Failing the draw makes us require a commit.
    state.didDrawIfPossibleCompleted(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    EXPECT_TRUE(state.redrawPending());
}

TEST(CCSchedulerStateMachineTest, TestCommitAfterFailedDrawAllowsDrawInSameFrame)
{
    CCSchedulerStateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Start a commit.
    state.setNeedsCommit();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_TRUE(state.commitPending());

    // Then initiate a draw.
    state.setNeedsRedraw();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    EXPECT_TRUE(state.redrawPending());

    // Fail the draw.
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.didDrawIfPossibleCompleted(false);
    EXPECT_TRUE(state.redrawPending());
    // But the commit is ongoing.
    EXPECT_TRUE(state.commitPending());

    // Finish the commit.
    state.beginFrameComplete(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES);
    state.updateResourcesComplete();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_COMMIT);
    EXPECT_TRUE(state.redrawPending());

    // And we should be allowed to draw again.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestCommitAfterFailedAndSuccessfulDrawDoesNotAllowDrawInSameFrame)
{
    CCSchedulerStateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Start a commit.
    state.setNeedsCommit();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_TRUE(state.commitPending());

    // Then initiate a draw.
    state.setNeedsRedraw();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    EXPECT_TRUE(state.redrawPending());

    // Fail the draw.
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.didDrawIfPossibleCompleted(false);
    EXPECT_TRUE(state.redrawPending());
    // But the commit is ongoing.
    EXPECT_TRUE(state.commitPending());

    // Force a draw.
    state.setNeedsForcedRedraw();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_FORCED, state.nextAction());

    // Do the forced draw.
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW_FORCED);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    EXPECT_FALSE(state.redrawPending());
    // And the commit is still ongoing.
    EXPECT_TRUE(state.commitPending());

    // Finish the commit.
    state.beginFrameComplete(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES);
    state.updateResourcesComplete();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_COMMIT);
    EXPECT_TRUE(state.redrawPending());

    // And we should not be allowed to draw again in the same frame..
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestFailedDrawsWillEventuallyForceADrawAfterTheNextCommit)
{
    CCSchedulerStateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);
    state.setMaximumNumberOfFailedDrawsBeforeDrawIsForced(1);

    // Start a commit.
    state.setNeedsCommit();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_TRUE(state.commitPending());

    // Then initiate a draw.
    state.setNeedsRedraw();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    EXPECT_TRUE(state.redrawPending());

    // Fail the draw.
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.didDrawIfPossibleCompleted(false);
    EXPECT_TRUE(state.redrawPending());
    // But the commit is ongoing.
    EXPECT_TRUE(state.commitPending());

    // Finish the commit. Note, we should not yet be forcing a draw, but should
    // continue the commit as usual.
    state.beginFrameComplete(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES);
    state.updateResourcesComplete();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_COMMIT);
    EXPECT_TRUE(state.redrawPending());

    // The redraw should be forced in this case.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_FORCED, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestFailedDrawIsRetriedNextVSync)
{
    CCSchedulerStateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Start a draw.
    state.setNeedsRedraw();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    EXPECT_TRUE(state.redrawPending());

    // Fail the draw.
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.didDrawIfPossibleCompleted(false);
    EXPECT_TRUE(state.redrawPending());

    // We should not be trying to draw again now, but we have a commit pending.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());

    state.didLeaveVSync();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();

    // We should try draw again in the next vsync.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestDoestDrawTwiceInSameFrame)
{
    CCSchedulerStateMachine state;
    state.setVisible(true);
    state.setCanDraw(true);
    state.setNeedsRedraw();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);

    // While still in the same vsync callback, set needs redraw again.
    // This should not redraw.
    state.setNeedsRedraw();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Move to another frame. This should now draw.
    state.didDrawIfPossibleCompleted(true);
    state.didLeaveVSync();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();

    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    state.didDrawIfPossibleCompleted(true);
    EXPECT_FALSE(state.vsyncCallbackNeeded());
}

TEST(CCSchedulerStateMachineTest, TestNextActionDrawsOnVSync)
{
    // When not on vsync, or on vsync but not visible, don't draw.
    size_t numCommitStates = sizeof(allCommitStates) / sizeof(CCSchedulerStateMachine::CommitState);
    for (size_t i = 0; i < numCommitStates; ++i) {
        for (unsigned j = 0; j < 2; ++j) {
            StateMachine state;
            state.setCommitState(allCommitStates[i]);
            bool visible = j;
            if (!visible) {
                state.didEnterVSync();
                state.setVisible(false);
            } else
                state.setVisible(true);

            // Case 1: needsCommit=false
            state.setNeedsCommit(false);
            EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());

            // Case 2: needsCommit=true
            state.setNeedsCommit(true);
            EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
        }
    }

    // When on vsync, or not on vsync but needsForcedRedraw set, should always draw except if you're ready to commit, in which case commit.
    for (size_t i = 0; i < numCommitStates; ++i) {
        for (unsigned j = 0; j < 2; ++j) {
            StateMachine state;
            state.setCanDraw(true);
            state.setCommitState(allCommitStates[i]);
            bool forcedDraw = j;
            if (!forcedDraw) {
                state.didEnterVSync();
                state.setNeedsRedraw(true);
                state.setVisible(true);
            } else
                state.setNeedsForcedRedraw(true);

            CCSchedulerStateMachine::Action expectedAction;
            if (allCommitStates[i] != CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT)
                expectedAction = forcedDraw ? CCSchedulerStateMachine::ACTION_DRAW_FORCED : CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE;
            else
                expectedAction = CCSchedulerStateMachine::ACTION_COMMIT;

            // Case 1: needsCommit=false updateMoreResourcesPending=false.
            state.setNeedsCommit(false);
            state.setUpdateResourcesCompletePending(false);
            EXPECT_TRUE(state.vsyncCallbackNeeded());
            EXPECT_EQ(expectedAction, state.nextAction());

            // Case 2: needsCommit=false updateMoreResourcesPending=true.
            state.setNeedsCommit(false);
            state.setUpdateResourcesCompletePending(true);
            EXPECT_TRUE(state.vsyncCallbackNeeded());
            EXPECT_EQ(expectedAction, state.nextAction());

            // Case 3: needsCommit=true updateMoreResourcesPending=false.
            state.setNeedsCommit(true);
            state.setUpdateResourcesCompletePending(false);
            EXPECT_TRUE(state.vsyncCallbackNeeded());
            EXPECT_EQ(expectedAction, state.nextAction());

            // Case 4: needsCommit=true updateMoreResourcesPending=true.
            state.setNeedsCommit(true);
            state.setUpdateResourcesCompletePending(true);
            EXPECT_TRUE(state.vsyncCallbackNeeded());
            EXPECT_EQ(expectedAction, state.nextAction());
        }
    }
}

TEST(CCSchedulerStateMachineTest, TestNoCommitStatesRedrawWhenInvisible)
{
    size_t numCommitStates = sizeof(allCommitStates) / sizeof(CCSchedulerStateMachine::CommitState);
    for (size_t i = 0; i < numCommitStates; ++i) {
        // There shouldn't be any drawing regardless of vsync.
        for (unsigned j = 0; j < 2; ++j) {
            StateMachine state;
            state.setCommitState(allCommitStates[i]);
            state.setVisible(false);
            state.setNeedsRedraw(true);
            state.setNeedsForcedRedraw(false);
            if (j == 1)
                state.didEnterVSync();

            // Case 1: needsCommit=false updateMoreResourcesPending=false.
            state.setNeedsCommit(false);
            state.setUpdateResourcesCompletePending(false);
            EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());

            // Case 2: needsCommit=false updateMoreResourcesPending=true.
            state.setNeedsCommit(false);
            state.setUpdateResourcesCompletePending(true);
            EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());

            // Case 3: needsCommit=true updateMoreResourcesPending=false.
            state.setNeedsCommit(true);
            state.setUpdateResourcesCompletePending(false);
            EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());

            // Case 4: needsCommit=true updateMoreResourcesPending=true.
            state.setNeedsCommit(true);
            state.setUpdateResourcesCompletePending(true);
            EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
        }
    }
}

TEST(CCSchedulerStateMachineTest, TestCanRedraw_StopsDraw)
{
    size_t numCommitStates = sizeof(allCommitStates) / sizeof(CCSchedulerStateMachine::CommitState);
    for (size_t i = 0; i < numCommitStates; ++i) {
        // There shouldn't be any drawing regardless of vsync.
        for (unsigned j = 0; j < 2; ++j) {
            StateMachine state;
            state.setCommitState(allCommitStates[i]);
            state.setVisible(false);
            state.setNeedsRedraw(true);
            state.setNeedsForcedRedraw(false);
            if (j == 1)
                state.didEnterVSync();

            state.setCanDraw(false);
            EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
        }
    }
}

TEST(CCSchedulerStateMachineTest, TestCanRedrawWithWaitingForFirstDrawMakesProgress)
{
    StateMachine state;
    state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW);
    state.setCanBeginFrame(true);
    state.setNeedsCommit(true);
    state.setNeedsRedraw(true);
    state.setUpdateResourcesCompletePending(false);
    state.setVisible(true);
    state.setCanDraw(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestVsyncCallbackNeededOnCanDrawAndResourceUpdates)
{
    StateMachine state;
    state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW);
    state.setCanBeginFrame(true);
    state.setNeedsCommit(true);
    state.setNeedsRedraw(true);
    state.setUpdateResourcesCompletePending(false);
    state.setVisible(true);
    state.setCanDraw(false);
    EXPECT_FALSE(state.vsyncCallbackNeeded());

    state.setUpdateResourcesCompletePending(true);
    EXPECT_TRUE(state.vsyncCallbackNeeded());

    state.setUpdateResourcesCompletePending(false);
    EXPECT_FALSE(state.vsyncCallbackNeeded());

    state.setCanDraw(true);
    EXPECT_TRUE(state.vsyncCallbackNeeded());
}

TEST(CCSchedulerStateMachineTest, TestUpdates_NoRedraw_OneRoundOfUpdates)
{
    StateMachine state;
    state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES);
    state.setNeedsRedraw(false);
    state.setUpdateResourcesCompletePending(false);
    state.setVisible(true);
    state.setCanDraw(true);

    // Verify we begin update, both for vsync and not vsync.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES, state.nextAction());
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES, state.nextAction());

    // Begin an update.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES);

    // Verify we don't do anything, both for vsync and not vsync.
    state.didLeaveVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // End update with no more updates pending.
    state.updateResourcesComplete();
    state.didLeaveVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestVSyncNeededWhenUpdatesPendingButInvisible)
{
    StateMachine state;
    state.setCanDraw(true);
    state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES);
    state.setNeedsRedraw(false);
    state.setVisible(false);
    state.setUpdateResourcesCompletePending(true);
    EXPECT_TRUE(state.vsyncCallbackNeeded());

    state.setUpdateResourcesCompletePending(false);
    EXPECT_TRUE(state.vsyncCallbackNeeded());
}

TEST(CCSchedulerStateMachineTest, TestUpdates_WithRedraw_OneRoundOfUpdates)
{
    StateMachine state;
    state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES);
    state.setNeedsRedraw(true);
    state.setUpdateResourcesCompletePending(false);
    state.setVisible(true);
    state.setCanDraw(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES, state.nextAction());

    // Begin an update.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES);

    // Ensure we draw on the next vsync even though an update is in-progress.
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    state.didDrawIfPossibleCompleted(true);

    // Ensure that we once we have drawn, we dont do anything else.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Leave the vsync before we finish the update.
    state.didLeaveVSync();
    state.updateResourcesComplete();

    // Verify we commit regardless of vsync state
    state.didLeaveVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestSetNeedsCommitIsNotLost)
{
    StateMachine state;
    state.setCanBeginFrame(true);
    state.setNeedsCommit(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Begin the frame.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(state.nextAction());
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());

    // Now, while the frame is in progress, set another commit.
    state.setNeedsCommit(true);
    EXPECT_TRUE(state.needsCommit());

    // Let the frame finish.
    state.beginFrameComplete(true);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.updateResourcesComplete();
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());

    // Expect to commit regardless of vsync state.
    state.didLeaveVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());

    // Commit and make sure we draw on next vsync
    state.updateState(CCSchedulerStateMachine::ACTION_COMMIT);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    state.didDrawIfPossibleCompleted(true);

    // Verify that another commit will begin.
    state.didLeaveVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestFullCycle)
{
    StateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Start clean and set commit.
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());

    // Begin the frame.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
    EXPECT_FALSE(state.needsCommit());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Tell the scheduler the frame finished.
    state.beginFrameComplete(true);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES, state.nextAction());

    // Tell the scheduler the update began and finished
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES);
    state.updateResourcesComplete();
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());

    // Commit.
    state.updateState(CCSchedulerStateMachine::ACTION_COMMIT);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());
    EXPECT_TRUE(state.needsRedraw());

    // Expect to do nothing until vsync.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // At vsync, draw.
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    state.didDrawIfPossibleCompleted(true);
    state.didLeaveVSync();

    // Should be synchronized, no draw needed, no action needed.
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_FALSE(state.needsRedraw());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestFullCycleWithCommitRequestInbetween)
{
    StateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Start clean and set commit.
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());

    // Begin the frame.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
    EXPECT_FALSE(state.needsCommit());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Request another commit while the commit is in flight.
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Tell the scheduler the frame finished.
    state.beginFrameComplete(true);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES, state.nextAction());

    // Tell the scheduler the update began and finished
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES);
    state.updateResourcesComplete();
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());

    // Commit.
    state.updateState(CCSchedulerStateMachine::ACTION_COMMIT);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());
    EXPECT_TRUE(state.needsRedraw());

    // Expect to do nothing until vsync.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // At vsync, draw.
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    state.didDrawIfPossibleCompleted(true);
    state.didLeaveVSync();

    // Should be synchronized, no draw needed, no action needed.
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_FALSE(state.needsRedraw());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestRequestCommitInvisible)
{
    StateMachine state;
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestGoesInvisibleBeforeBeginFrameCompletes)
{
    StateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Start clean and set commit.
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());

    // Begin the frame while visible.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
    EXPECT_FALSE(state.needsCommit());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Become invisible and abort the beginFrame.
    state.setVisible(false);
    state.beginFrameAborted();

    // We should now be back in the idle state as if we didn't start a frame at all.
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Become visible again
    state.setVisible(true);

    // We should be beginning a frame now
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());

    // Begin the frame
    state.updateState(state.nextAction());

    // We should be starting the commit now
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
}

TEST(CCSchedulerStateMachineTest, TestContextLostWhenCompletelyIdle)
{
    StateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    state.didLoseContext();

    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_CONTEXT_RECREATION, state.nextAction());
    state.updateState(state.nextAction());

    // Once context recreation begins, nothing should happen.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Recreate the context
    state.didRecreateContext();

    // When the context is recreated, we should begin a commit
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestContextLostWhenIdleAndCommitRequestedWhileRecreating)
{
    StateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    state.didLoseContext();

    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_CONTEXT_RECREATION, state.nextAction());
    state.updateState(state.nextAction());

    // Once context recreation begins, nothing should happen.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // While context is recreating, commits shouldn't begin.
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Recreate the context
    state.didRecreateContext();

    // When the context is recreated, we should begin a commit
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(state.nextAction());

    // Once the context is recreated, whether we draw should be based on
    // setCanDraw.
    state.setNeedsRedraw(true);
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.setCanDraw(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.setCanDraw(true);
    state.didLeaveVSync();
}

TEST(CCSchedulerStateMachineTest, TestContextLostWhileCommitInProgress)
{
    StateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Get a commit in flight.
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(state.nextAction());

    // Set damage and expect a draw.
    state.setNeedsRedraw(true);
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(state.nextAction());
    state.didLeaveVSync();

    // Cause a lost context while the begin frame is in flight.
    state.didLoseContext();

    // Ask for another draw. Expect nothing happens.
    state.setNeedsRedraw(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Finish the frame, update resources, and commit.
    state.beginFrameComplete(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES, state.nextAction());
    state.updateState(state.nextAction());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.updateResourcesComplete();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.updateState(state.nextAction());

    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());

    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(state.nextAction());

    // Expect to be told to begin context recreation, independent of vsync state
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_CONTEXT_RECREATION, state.nextAction());
    state.didLeaveVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_CONTEXT_RECREATION, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestContextLostWhileCommitInProgressAndAnotherCommitRequested)
{
    StateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Get a commit in flight.
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(state.nextAction());

    // Set damage and expect a draw.
    state.setNeedsRedraw(true);
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(state.nextAction());
    state.didLeaveVSync();

    // Cause a lost context while the begin frame is in flight.
    state.didLoseContext();

    // Ask for another draw and also set needs commit. Expect nothing happens.
    state.setNeedsRedraw(true);
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Finish the frame, update resources, and commit.
    state.beginFrameComplete(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES, state.nextAction());
    state.updateState(state.nextAction());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.updateResourcesComplete();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.updateState(state.nextAction());

    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());

    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(state.nextAction());

    // Expect to be told to begin context recreation, independent of vsync state
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_CONTEXT_RECREATION, state.nextAction());
    state.didLeaveVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_CONTEXT_RECREATION, state.nextAction());
}


TEST(CCSchedulerStateMachineTest, TestFinishAllRenderingWhileContextLost)
{
    StateMachine state;
    state.setVisible(true);
    state.setCanDraw(true);

    // Cause a lost context lost.
    state.didLoseContext();

    // Ask a forced redraw and verify it ocurrs.
    state.setNeedsForcedRedraw(true);
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_FORCED, state.nextAction());
    state.didLeaveVSync();

    // Clear the forced redraw bit.
    state.setNeedsForcedRedraw(false);

    // Expect to be told to begin context recreation, independent of vsync state
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_CONTEXT_RECREATION, state.nextAction());
    state.updateState(state.nextAction());

    // Ask a forced redraw and verify it ocurrs.
    state.setNeedsForcedRedraw(true);
    state.didEnterVSync();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW_FORCED, state.nextAction());
    state.didLeaveVSync();
}

TEST(CCSchedulerStateMachineTest, TestBeginFrameWhenInvisibleAndForceCommit)
{
    StateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(false);
    state.setNeedsCommit(true);
    state.setNeedsForcedCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestBeginFrameWhenCanBeginFrameFalseAndForceCommit)
{
    StateMachine state;
    state.setVisible(true);
    state.setCanDraw(true);
    state.setNeedsCommit(true);
    state.setNeedsForcedCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestBeginFrameWhenCommitInProgress)
{
    StateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(false);
    state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS);
    state.setNeedsCommit(true);
    state.setNeedsForcedCommit(true);

    state.beginFrameComplete(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_RESOURCES, state.nextAction());
    state.updateState(state.nextAction());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.updateResourcesComplete();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.updateState(state.nextAction());

    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());

    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestBeginFrameWhenContextLost)
{
    StateMachine state;
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);
    state.setNeedsCommit(true);
    state.setNeedsForcedCommit(true);
    state.didLoseContext();
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

}
