<?php
// $Id$

/**
 * @file
 * Implementation of migration from WordPress into Drupal
 */
abstract class WordPressMigration extends Migration {
  /**
   * The filespec of the WXR file this migration is based on.
   *
   * @var string
   */
  protected $wxrFile;

  /**
   * The implemented WordPress migrations, in the order they should be run.
   */
  public static function migrationList() {
    return array(
      'WordPressCategoryMigration',
      'WordPressBlogEntryMigration',
      'WordPressPageMigration',
      'WordPressCommentMigration',
    );
  }

  /**
   * Constructor - general setup for WordPress migrations.
   *
   * @param array $arguments
   *  'filename' => WXR file managed by this migration
   */
  public function __construct(array $arguments) {
    if (!$arguments['filename']) {
      throw new Exception(t('Filename is a required parameter for WordPress migrations'));
    }

    // Must be set before calling the parent constructor, which will call
    // generateMachineName() below.
    $this->wxrFile = $arguments['filename'];
    parent::__construct($arguments);
  }

  /**
   * Construct the machine name from the blog title
   *
   * @param string $class
   *  Class name to use in constructing the machine name. Defaults to the current
   *  class name.
   */
  protected function generateMachineName($class = NULL) {
    $title = db_select('wordpress_migrate', 'wm')
             ->fields('wm', array('title'))
             ->condition('filename', $this->wxrFile)
             ->execute()
             ->fetchField();
    if (!$class) {
      $class = get_class($this);
    }
    // The machine name is the cleansed blog title, followed by the portion
    // of the class name between WordPress and Migration. For example, for
    // category migration (class WordPressCategoryMigration) of "Mike's Blog",
    // the generated machine name will be MikesBlogCategory.
    return $title . substr($class, strlen('WordPress'),
      strlen($class) - strlen('Migration') - strlen('WordPress'));
  }
}