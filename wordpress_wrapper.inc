<?php

/**
 * Wordpress Migrate (wordpress://) stream wrapper class.
 *
 * Provides support for storing uploaded WXR files where they won't be seen by
 * anyone but the import code.
 *
 * Extends BackdropLocalStreamWrapper.
 */
class WordpressMigrateStreamWrapper extends BackdropLocalStreamWrapper {
  /**
   * Implements abstract public function getDirectoryPath()
   */
  public function getDirectoryPath() {
    $config = config('wordpress_migrate.settings');
    // Allow override with an explicit path, not necessarily relative to
    // file_private_path
    $private_path = $config->get('wordpress_migrate_private_path');
    if (!$private_path) {
      $private_path = config_get('system.performance', 'file_private_path') . '/wordpress';
    }
    return $private_path;
  }

  /**
   * Overrides getExternalUrl().
   *
   * Do not allow an external URL to WordPress files.
   */
  function getExternalUrl() {
    return FALSE;
  }
}
