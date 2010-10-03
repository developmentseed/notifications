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
}

/**
 * Node type subscription
 */
class Notifications_Node_Type_Subscription extends Notifications_Subscription_Type {
  public $field_types = array('node:type');
  public $field_values = array(NULL);
}

/**
 * Author subscription
 */
class Notifications_Author_Subscription extends Notifications_Subscription_Type {
  public $field_types = array('node:uid');
  public $field_values = array(NULL);
}
/**
 * Node type and author subscription
 */
class Notifications_Node_Type_Author_Subscription extends Notifications_Subscription_Type {
  public $field_types = array('node:type', 'node:uid');
  public $field_values = array(NULL, NULL);
}

/**
 * Node events
 */
class Notifications_Node_Event extends Notifications_Event {
 
  /**
   * Get node object
   */
  public function get_node() {
    return $this->get_object('node');
  }
  /**
   * Trigger node event
   */
  public function trigger() {
    $node = $this->get_node();
    parent::trigger();
    watchdog('action', 'Triggered notifications for  @type %title..', array('@type' => node_type_get_name($node), '%title' => $node->title));
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
    
  }
}
  
/**
 * Event types: When a (published) node is created or an existing node is published
 */
class Notifications_Node_Post_Event extends Notifications_Node_Event {
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
class Notifications_Node_Event_Template extends Notifications_Template {

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
    t('The [type-name] has been updated by [author-name]');
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

/**
 * 
 */