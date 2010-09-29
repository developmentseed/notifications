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
  public $field_types = array('node');
  public $field_values = array(NULL);
}

/**
 * Node events
 */
class Notifications_Node_Event extends Notifications_Event {
  
  /**
   * Build from node and action
   * 
   * @param $action
   *   Node action: 'insert', 
   */
  public static function build($node, $action) {
    if ($class = notifications_event_type('node-' . $node->type . '-' . $action, 'class')) {
      $type = 'node-' . $node->type . '-' . $action;
    }
    else if ($class = notifications_event_type('node-' . $action, 'class')) {
      $type = 'node-' . $action;
    }
    $node = notifications_object('node', $node);
  }
}

class Notifications_Comment extends Notifications_Object {
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

class Notifications_Node_Type_Field extends Notifications_Field_Select {
  public $type = 'node:type';
  /**
   * Select options, content type list
   */
  public function select_options () {
    
  }
}
  
/**
 * Event types
 */
class Notifications_Node_Insert_Event extends Notifications_Node_Event {
  public function get_name() {
    return t('New content of type [type-name] has been submitted');
  }
}
class Notifications_Node_Update_Event extends Notifications_Node_Event {
  public function get_name() {
    return t('The [type-name] has been updated');
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
class Notifications_Node_Event_Template extends Notifications_Message_Template {

}

/**
 * Template for node inserts
 */
class Notifications_Node_Insert_Template extends Notifications_Node_Event_Template {
  public function get_title() {
    return t('Template for node posts');
  }  
  public function digest_fields() {
    return array('node:nid', 'node:type');
  }
  public function digest_line() {
    return t('[type-name] [title] by [author-name]');
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
  public function digest_line() {
    t('The [type-name] has been updated by [author-name]')
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
  public function digest_line() {
    return t('New comment by [comment-author-name]: [comment-title]');
  }  
}
