<?php
// $Id$

/**
 * @file
 *
 * Support for migrating items from a WordPress blog into Drupal.
 */

/**
 * Implementation of MigrateSource, to handle migrating items from WordPress XML dumps.
 */
class WordPressItemSource extends WordPressSource {
  /**
   * The <wp:post_type> value we're looking for in this migration
   * (post/page/attachment).
   *
   * @var string
   */
  protected $postType;

  /**
   * List of available source fields.
   *
   * @var array
   */
  protected $fields = array(
    'title' => 'Item title',
    'link' => 'WordPress URL of the item',
    'pubDate' => 'Published date',
    'creator' => 'WordPress username of the item author',
    'guid' => 'Alternate URL of the item (?)',
    'description' => '?',
    'content' => 'Body of the item',
    'excerpt' => 'Teaser for the item',
    'post_id' => 'Unique ID of the item within the blog',
    'post_date' => 'Date posted (author\s timezone?)',
    'post_date_gmt' => 'Date posted (GMT)',
    'comment_status' => 'Whether comments may be posted to this item (open/closed)',
    'ping_status' => '?',
    'post_name' => 'Trailing component of link',
    'status' => 'Item status (publish/draft/inherit)',
    'post_parent' => 'Parent item ID (?)',
    'menu_order' => 'Equivalent to Drupal weight?',
    'post_type' => 'Item type (post/page/attachment)',
    'post_password' => '?',
    'is_sticky' => 'Equivalent to Drupal sticky flag',
    'category' => 'Categories (as nicename) assigned to this item',
  );

  /**
   * Simple initialization.
   *
   */
  public function __construct($filename, $post_type) {
    parent::__construct($filename);
    $this->postType = $post_type;
    $this->xpath = '//channel/item';
  }

  /**
   * Return a count of all available source records.
   *
   * @param boolean $refresh
   *  Not currently in use.
   */
  public function count($refresh = FALSE) {
    $post_types = $this->xml->xpath("//channel/item[wp:post_type='$this->postType']");
    $count = count($post_types);
    return $count;
  }

  /**
   * Parse the values out of the item element.
   *
   * @param SimpleXMLElement $item
   * @return boolean
   */
  protected function populateRow($item) {
    // Pull non-namespaced items
    foreach ($item as $name => $value) {
      $this->currentRow->$name = (string)$value;
    }
    // Now check each namespace
    $namespaces = $item->getNameSpaces(TRUE);

    foreach ($namespaces as $ns => $nsuri) {
      $item_ns = $item->children($namespaces[$ns]);
      foreach ($item_ns as $name => $value) {
        // Special-case content:encoded and excerpt:encoded, which otherwise
        // would both be saved as "encoded"
        if ($name == 'encoded') {
          $this->currentRow->$ns = (string)$value;
        }
        else {
          $this->currentRow->$name = (string)$value;
        }
      }
    }
    // Make sure this is the desired post type
    if ($this->currentRow->post_type != $this->postType) {
      $this->currentRow = NULL;
      return FALSE;
    }

    // The category field is now the descriptive name, we want the nicename
    $this->currentRow->category = array();
    foreach ($item->category as $cat) {
      foreach ($cat->attributes() as $name => $attribute) {
        if ($name == 'nicename') {
          $this->currentRow->category[] = (string)$attribute;
        }
      }
    }

    $this->currentKey = array($this->currentRow->post_id);
    return TRUE;
  }
}

/**
 * Intermediate Migration class, implementing behavior common across different
 * types (post_type) of items.
 */
abstract class WordPressItemMigration extends WordPressMigration {
  /**
   * Set it up
   */
  public function __construct(array $arguments = array()) {
    parent::__construct($arguments);

    // WordPress post type
    $post_type = $arguments['post_type'];
    // Drupal content type
    $node_type = $arguments['node_type'];

    // post_id is the unique ID of items in WordPress
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'post_id' => array(
          'type' => 'int',
          'not null' => TRUE,
          'unsigned' => TRUE,
          'description' => 'WordPress post ID',
        )
      ),
      MigrateDestinationNode::getKeySchema()
    );

    // Construct the source and destination objects.
    $this->source = new WordPressItemSource($this->wxrFile, $post_type);
    $this->destination = new MigrateDestinationNode($node_type);

    // Default mappings, applying to most or all migrations
    $this->addFieldMapping('title', 'title');
    $this->addFieldMapping('created', 'post_date')
         ->description('Empty dates handled in prepare()');
    $this->addFieldMapping('changed', 'post_date')
         ->description('Empty dates handled in prepare()');
    $this->addFieldMapping('uid')
         ->description('For now, current user (anonymous user when run in drush)')
         ->issueGroup(t('Open issues'))
         ->issuePriority(MigrateFieldMapping::ISSUE_PRIORITY_MEDIUM);
    $this->addFieldMapping(NULL, 'creator')
         ->description('Look for matching Drupal username')
         ->issueGroup(t('Open issues'))
         ->issuePriority(MigrateFieldMapping::ISSUE_PRIORITY_MEDIUM);
    $arguments = MigrateTextFieldHandler::arguments(array('source_field' => 'excerpt'));
    $this->addFieldMapping('body', 'content')
         ->description('What input format?')
         ->issueGroup(t('Open issues'))
         ->issuePriority(MigrateFieldMapping::ISSUE_PRIORITY_MEDIUM)
         ->arguments($arguments);
    $this->addFieldMapping(NULL, 'excerpt');
    $this->addFieldMapping('comment', 'comment_status')
         ->description('WP "open" mapped to Drupal COMMENT_NODE_OPEN');
    $this->addFieldMapping('status', 'status')
         ->description('Set Drupal status to 1 iff wp:status=publish');
    $this->addFieldMapping(NULL, 'post_parent')
         ->description('For attachments, indicates item attached to')
         ->issueGroup(t('Open issues'))
         ->issuePriority(MigrateFieldMapping::ISSUE_PRIORITY_MEDIUM);
    $this->addFieldMapping('sticky', 'is_sticky');
    if (module_exists('path')) {
      $this->addFieldMapping('path', 'link')
           ->description(t('If no ? in the link, strip the domain for the path'));
      if (module_exists('pathauto')) {
        $this->addFieldMapping('pathauto_perform_alias')
             ->defaultValue(0)
             ->description('Disable pathauto, we set the alias from the WP link - not working, why?')
             ->issueGroup(t('Open issues'))
             ->issuePriority(MigrateFieldMapping::ISSUE_PRIORITY_MEDIUM);
      }
    }

    // Unmapped destination fields
    $this->addFieldMapping('is_new')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('revision')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('language')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping('promote')
         ->issueGroup(t('DNM'));

    // Unmapped source fields
    $this->addFieldMapping(NULL, 'guid')
         ->description('same as link, plus isPermaLink attribute?')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'description')
         ->description('Always empty?')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'post_id')
         ->description(t('Primary key of source'))
         ->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'pubDate')
         ->description('Use post_date')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'post_date_gmt')
         ->description('Use post_date')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'ping_status')
         ->description('What does this mean?')
         ->issueGroup(t('Open issues'))
         ->issuePriority(MigrateFieldMapping::ISSUE_PRIORITY_MEDIUM);
    $this->addFieldMapping(NULL, 'post_name')
         ->description('Looks like last component of path')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'menu_order')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'post_type')
         ->issueGroup(t('DNM'));
    $this->addFieldMapping(NULL, 'post_password')
         ->description('???')
         ->issueGroup(t('DNM'));
  }

  /**
   * Data manipulations to be performed before migrate module applies mappings.
   *
   * @param stdClass $row
   * @return string
   */
  public function prepareRow($row) {
    // Only publish those with wp:status == 'publish'
    if (isset($row->status) && $row->status == 'publish') {
      $row->status = NODE_PUBLISHED;
    }
    else {
      $row->status = NODE_NOT_PUBLISHED;
    }

    // If incoming date is zero (indicates unpublished content), use the current time
    if ($row->post_date == '0000-00-00 00:00:00') {
      $row->post_date = time();
    }

    // If the link has a query string, don't produce a path
    if (strpos($row->link, '?')) {
      unset($row->link);
    }
    else {
      // Otherwise, strip the domain portion of the URL
      $matches = array();
      if (preg_match('|http://[^/]+/(.*)|', $row->link, $matches)) {
        $row->link = $matches[1];
      }
      else {
        unset($row->link);
      }
    }

    // Translate WordPress comment_status to Drupal values
    if ($row->comment_status == 'open') {
      $row->comment_status = COMMENT_NODE_OPEN;
    }
    else {
      $row->comment_status = COMMENT_NODE_CLOSED;
    }
    return TRUE;
  }
}

/**
 * Implementation of WordPressMigration, for blog entries
 */
class WordPressBlogEntryMigration extends WordPressItemMigration {
  public function __construct(array $arguments = array()) {
    $arguments['post_type'] = 'post';
    $arguments['node_type'] = 'blog';
    parent::__construct($arguments);

    // Blog-specific mappings
    $this->addFieldMapping('field_tags', 'category');
  }
}

/**
 * Implementation of WordPressMigration, for pages
 */
class WordPressPageMigration extends WordPressItemMigration {
  public function __construct(array $arguments = array()) {
    $arguments['post_type'] = 'page';
    $arguments['node_type'] = 'page';
    parent::__construct($arguments);

    // Page-specific mappings
    $this->addFieldMapping(NULL, 'category')
         ->description('WP pages do have categories, but we don\'t have a vocabulary in Drupal')
         ->issueGroup(t('DNM'));
  }
}

/**
 * TODO: Implementation of WordPressMigration, for attachments
 */