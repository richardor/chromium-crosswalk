# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //apps
      'target_name': 'apps',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      # Since browser and browser_extensions actually depend on each other,
      # we must omit the dependency from browser_extensions to browser.
      # However, this means browser_extensions and browser should more or less
      # have the same dependencies. Once browser_extensions is untangled from
      # browser, then we can clean up these dependencies.
      'dependencies': [
        'browser_extensions',
        'chrome_resources.gyp:theme_resources',
        'common/extensions/api/api.gyp:chrome_api',
        '../skia/skia.gyp:skia',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(grit_out_dir)',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'app_lifetime_monitor.cc',
        'app_lifetime_monitor.h',
        'app_lifetime_monitor_factory.cc',
        'app_lifetime_monitor_factory.h',
        'app_load_service.cc',
        'app_load_service.h',
        'app_load_service_factory.cc',
        'app_load_service_factory.h',
        'app_restore_service.cc',
        'app_restore_service.h',
        'app_restore_service_factory.cc',
        'app_restore_service_factory.h',
        'app_window.cc',
        'app_window.h',
        'app_window_contents.cc',
        'app_window_contents.h',
        'app_window_registry.cc',
        'app_window_registry.h',
        'browser_context_keyed_service_factories.cc',
        'browser_context_keyed_service_factories.h',
        'custom_launcher_page_contents.cc',
        'custom_launcher_page_contents.h',
        'launcher.cc',
        'launcher.h',
        'metrics_names.h',
        'pref_names.cc',
        'pref_names.h',
        'prefs.cc',
        'prefs.h',
        'saved_files_service.cc',
        'saved_files_service.h',
        'saved_files_service_factory.cc',
        'saved_files_service_factory.h',
        'switches.cc',
        'switches.h',
        'ui/apps_client.cc',
        'ui/apps_client.h',
        'ui/views/app_window_frame_view.cc',
        'ui/views/app_window_frame_view.h',
        'ui/views/native_app_window_views.cc',
        'ui/views/native_app_window_views.h',
        'ui/web_contents_sizer.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources': [
            'ui/web_contents_sizer.mm',
          ],
        }, {  # OS!=mac
          'sources': [
            'ui/web_contents_sizer.cc',
          ],
        }],
        ['chromeos==1',
          {
            'dependencies': [
              'browser_chromeos',
            ]
          }
        ],
        ['enable_extensions==0',
          {
            'dependencies!': [
              'browser_extensions',
            ],
            'sources/': [
              ['exclude', '.*'],
              ['include', 'ui/web_contents_sizer\.cc$'],
              ['include', 'ui/web_contents_sizer\.mm$'],
            ],
          }
        ],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/strings/ui_strings.gyp:ui_strings',
            '../ui/views/views.gyp:views',
          ],
        }, {  # toolkit_views==0
          'sources/': [
            ['exclude', 'ui/views/'],
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],  # targets
}
