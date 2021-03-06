<?php
// $Id: notifications_subscription.class.inc,v 1.1.2.35 2010/07/21 10:55:30 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Thread subscription
 */
class Notifications_Node_Subscription extends Notifications_Subscription_Type {
  public $field_types = array('node:nid');
  public $field_values = array(NULL);
  public $object_types = array('node');
}

/**
 * Node type subscription
 */
class Notifications_Node_Type_Subscription extends Notifications_Subscription_Type {
  public $field_types = array('node:type');
  public $field_values = array(NULL);
  public $object_types = array('node', 'node_type');
}

/**
 * Author subscription
 */
class Notifications_Author_Subscription extends Notifications_Subscription_Type {
  public $field_types = array('node:uid');
  public $field_values = array(NULL);
  public $object_types = array('node', 'user');
}
/**
 * Node type and author subscription
 */
class Notifications_Node_Type_Author_Subscription extends Notifications_Subscription_Type {
  public $field_types = array('node:type', 'node:uid');
  public $field_values = array(NULL, NULL);
  public $object_types = array('node', 'node_type', 'user');
}

/**
 * Node events
 */
class Notifications_Node_Event extends Notifications_Event {
  /**
   * Set main object id when node is added to the event
   */
  public function set_object($object) {
    if ($object->type == 'node') {
      $this->oid = $object->get_value();
    }
    return parent::set_object($object);
  }
  /**
   * Get node object
   */
  public function get_node() {
    return $this->get_object('node')->get_object();
  }
  /**
   * Trigger node event
   */
  public function trigger() {
    $node = $this->get_node();
    if ($result = parent::trigger()) {
      watchdog('action', 'Triggered notifications for  @type %title..', array('@type' => node_type_get_name($node), '%title' => $node->title));
    }
    return $result;
  }
}

class Notifications_Comment extends Notifications_Object {
  public $type = 'comment';

  public function get_title() {
    return t('Comment');
  }
  /**
   * Load related object or data
   */
  public static function object_load($value) {  
    return comment_load($value);
  }
  /**
   * Get object name, unfiltered string
   */
  public static function object_name($object) {
    return $comment->title;
  }
  /**
   * Get object key
   */
  public static function object_value($object) {
    return $object->cid;
  }  
}

/**
 * Node type object
 */
class Notifications_Node_Type extends Notifications_Object {
  public $type = 'nodetype';
  
  public function get_title() {
    return t('Content type');
  }
  /**
   * Load related object or data
   */
  public static function object_load($value) {  
    return user_load($value);
  }
  /**
   * Get object name, unfiltered string
   */
  public static function object_name($object) {
    return $object->name;
  }
  /**
   * Get object key
   */
  public static function object_value($object) {
    return $object->type;
  }  
}

/**
 * Node type field
 */
class Notifications_Node_Type_Field extends Notifications_Field_Select {
  public $type = 'node:type';
  
  public function get_title() {
    return t('Content type');
  }
  public function get_description() {
    return t('Content type');
  }  
  /**
   * Select options, content type list
   */
  public function select_options () {
    return node_type_get_names();
  }
}
  
/**
 * Event types: When a (published) node is created or an existing node is published
 */
class Notifications_Node_Post_Event extends Notifications_Node_Event {
  public function get_name() {
    return t('New content of type [node:type-name] has been submitted');
  }
}
class Notifications_Node_Update_Event extends Notifications_Node_Event {
  public function get_name() {
    return t('The [node:type-name] has been updated');
  }  
}
class Notifications_Node_Comment_Event extends Notifications_Node_Event {
  public function get_name() {
    t('New comment by [comment-author-name]: [comment-title]');
  }
  
}

/**
 * Message templates for node events. Base class.
 */
class Notifications_Node_Event_Template extends Notifications_Template {
  /**
   * Default texts for this template, may need token replacement
   */
  function default_text($type, $options) {
    switch ($type) {
      case 'subject':
        return t('Update for [node:type-name]: [node:title]', array(), $options);
      case 'content':
        return array(
          'teaser' => '[node:summary]',
          'more' => t('Read more [node:url]', array(), $options),
        );
      case 'digest':
        return array(
          'title' => '[node:title]',
          'more' =>t('Read more [node:url]', array(), $options),
        );
      default:
        return parent::default_text($type, $options);
    }
  }
}

/**
 * Template for node inserts
 */
class Notifications_Node_Insert_Template extends Notifications_Node_Event_Template {
  public function get_title() {
    return t('Template for node posts');
  }  
  public function digest_fields() {
    return array('node:type', 'author:uid');
  }
  /**
   * New nodes can be digested by type or by author
   */
  public function digest_line($field, $options = array()) {
    switch ($field) {
      case 'node:type':
        return t('[node:title] by [author-name].', array(), $options);
      case 'author:uid':
        return t('New [node:type-name]: [node:title]', array(), $options);
    }
  }
    /**
   * Default texts for this template, may need token replacement
   */
  function default_text($type, $options) {
    switch ($type) {
      case 'subject':
        return t('New [node:type-name]: [node:title]', array(), $options);
      default:
        return parent::default_text($type, $options);
    }
  }
}

/**
 * Template for node updates
 */
class Notifications_Node_Update_Template extends Notifications_Node_Event_Template {
  public function get_title() {
    return t('Template for node updates');
  }  
  public function digest_fields() {
    return array('node:nid', 'author:uid');
  }
  public function digest_line($field, $options = array()) {
    switch ($field) {
      case 'node:nid':
        return t('The [node:type-name] has been updated by [user:name]', array(), $options);
      case 'author:uid':
        return t('Updated [node:type-name]: [node:title]', array(), $options);
    }    
  }  
}

/**
 * Template for node comments
 */
class Notifications_Node_Comment_Template extends Notifications_Node_Event_Template {
  public function get_title() {
    t('Template for node comments');
  }
  public function digest_fields() {
    return array('node:nid');
  }
  public function digest_line($field, $options = array()) {
    switch ($field) {
      case 'node:nid':
        return t('New comment by [comment:name]: [comment:title]', array(), $options);
      case 'author:uid':
        return t('Commented on [node:type-name]: [node:title]', array(), $options);
    }  
  }
  /**
   * Default texts for this template, may need token replacement
   */
  function default_text($type, $options) {
    switch ($type) {
      case 'subject':
        return t('Comment for [node:type-name]: [node:title]', array(), $options);
      case 'content':
        return array(
          'header' => t('Comment by [comment:name]: [comment:title]', array(), $options),
          'content' => '[comment:body]',
          'footer' => t('Read more [comment-url]', array(), $options),
        );
      case 'digest':
        return array(
          'title' => '[node:title]',
          'more' =>t('Read more [node:url]', array(), $options),
        );
      default:
        return parent::default_text($type, $options);
    }
  }
}
