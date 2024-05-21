# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os

from file_system import FileNotFoundError
import compiled_file_system as compiled_fs
from handlebar_dict_generator import HandlebarDictGenerator
import third_party.json_schema_compiler.json_comment_eater as json_comment_eater
import third_party.json_schema_compiler.model as model
import third_party.json_schema_compiler.idl_schema as idl_schema
import third_party.json_schema_compiler.idl_parser as idl_parser

class _LazySamplesGetter(object):
  """This class is needed so that an extensions API page does not have to fetch
  the apps samples page and vice versa.
  """
  def __init__(self, api_name, samples):
    self._api_name = api_name
    self._samples = samples

  def get(self, key):
    return self._samples.FilterSamples(key, self._api_name)

class APIDataSource(object):
  """This class fetches and loads JSON APIs from the FileSystem passed in with
  |cache_factory|, so the APIs can be plugged into templates.
  """
  class Factory(object):
    def __init__(self, cache_factory, base_path, samples_factory):
      self._permissions_cache = cache_factory.Create(self._LoadPermissions,
                                                     compiled_fs.PERMS)
      self._json_cache = cache_factory.Create(self._LoadJsonAPI,
                                              compiled_fs.JSON)
      self._idl_cache = cache_factory.Create(self._LoadIdlAPI, compiled_fs.IDL)
      self._idl_names_cache = cache_factory.Create(self._GetIDLNames,
                                                   compiled_fs.IDL_NAMES)
      self._samples_factory = samples_factory
      self._base_path = base_path

    def Create(self, request):
      return APIDataSource(self._permissions_cache,
                           self._json_cache,
                           self._idl_cache,
                           self._idl_names_cache,
                           self._base_path,
                           self._samples_factory.Create(request))

    def _LoadPermissions(self, json_str):
      return json.loads(json_comment_eater.Nom(json_str))

    def _LoadJsonAPI(self, api):
      return HandlebarDictGenerator(json.loads(json_comment_eater.Nom(api))[0])

    def _LoadIdlAPI(self, api):
      idl = idl_parser.IDLParser().ParseData(api)
      return HandlebarDictGenerator(idl_schema.IDLSchema(idl).process()[0])

    def _GetIDLNames(self, apis):
      return [model.UnixName(os.path.splitext(api.split('/')[-1])[0])
              for api in apis if api.endswith('.idl')]

  def __init__(self,
               permissions_cache,
               json_cache,
               idl_cache,
               idl_names_cache,
               base_path,
               samples):
    self._base_path = base_path
    self._permissions_cache = permissions_cache
    self._json_cache = json_cache
    self._idl_cache = idl_cache
    self._idl_names_cache = idl_names_cache
    self._samples = samples

  def _GetPermsFromFile(self, filename):
    try:
      perms = self._permissions_cache.GetFromFile('%s/%s' %
          (self._base_path, filename))
      return dict((model.UnixName(k), v) for k, v in perms.iteritems())
    except FileNotFoundError:
      return {}

  def _GetFeature(self, path):
    # Remove 'experimental_' from path name to match the keys in
    # _permissions_features.json.
    path = model.UnixName(path.replace('experimental_', ''))
    for filename in ['_permission_features.json', '_manifest_features.json']:
      api_perms = self._GetPermsFromFile(filename).get(path, None)
      if api_perms is not None:
        break
    if api_perms and api_perms['channel'] in ('trunk', 'dev', 'beta'):
      api_perms[api_perms['channel']] = True
    return api_perms

  def _GenerateHandlebarContext(self, handlebar, path):
    return_dict = {
      'permissions': self._GetFeature(path),
      'samples': _LazySamplesGetter(path, self._samples)
    }
    return_dict.update(handlebar.Generate())
    return return_dict

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    path, ext = os.path.splitext(key)
    unix_name = model.UnixName(path)
    idl_names = self._idl_names_cache.GetFromFileListing(self._base_path)
    cache, ext = ((self._idl_cache, '.idl') if (unix_name in idl_names) else
                  (self._json_cache, '.json'))
    return self._GenerateHandlebarContext(
        cache.GetFromFile('%s/%s%s' % (self._base_path, unix_name, ext)),
        path)
