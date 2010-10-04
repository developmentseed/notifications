<?php
// $Id: notifications_subscription.class.inc,v 1.1.2.35 2010/07/21 10:55:30 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */
class Notifications_Taxonomy_Term_Subscription extends Notifications_Subscription_Type {
  public $field_types = array('term:tid');
  public $field_values = array(NULL);

  public function get_title() {
    return t('Tag');
  }

}

/**
 * Taxonomy term object
 */
class Notifications_Taxonomy_Term extends Notifications_Object {
  public $type = 'taxonomy_term';

  /**
   * Title
   */
  public function get_title() {
    return t('Taxonomy term');
  }
  /**
   * Load object
   */  
  public static function object_load($value) {
    return taxonomy_term_load($value);
  }
  /**
   * Get name for object
   */
  public static function object_name($term) {
    return $term->name;
  }
  /**
   * Get value for object
   */
  public static function object_value($term) {
    return $term->tid;
  }
}

/**
 * Taxonomy term field
 */
class Notifications_Taxonomy_Term_Field extends Notifications_Field_Autocomplete {
  public $type = 'term:tid';
  public $object_type = 'taxonomy_term';
  /**
   * Get field name
   */
  function get_title() {
    return t('Term');
  }
  /**
   * Get description
   */
  function get_description() {
    return t('Taxonomy term');
  }
   /**
   * Get autocomplete path
   */
  function autocomplete_path() {
    return 'taxonomy/autocomplete';
  }

  /**
   * Parse value from autocomplete, get the first matching term
   */
  function autocomplete_parse($string) {
    if ($terms = taxonomy_get_term_by_name($string)) {
      $term = reset($terms);
      return $term->tid;
    }
  }
}

/**
 * Taxonomy vocabulary object
 */
class Notifications_Taxonomy_Vocabulary extends Notifications_Object {
  public $type = 'vocabulary';

  /**
   * Title
   */
  public function get_title() {
    return t('Vocabulary');
  }
  
  /**
   * Load object
   */
  public static function object_load($value) {
    return taxonomy_term_load($value);
  }
  /**
   * Get name for object
   */
  public static function object_name($vocabulary) {
    return $vocabulary->name;
  }
  /**
   * Get value for object
   */
  public static function object_value($vocabulary) {
    return $vocabulary->vid;
  }
}