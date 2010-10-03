<?php
// $Id: notifications_event.class.inc,v 1.1.2.13 2010/07/22 11:57:49 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */

/**
 * Notifications Event class
 * 
 * This is the object that when triggered will produce notifications to be sent.
 * 
 * Each event type can have a linked template that will be the object responsible for building the notification text
 * 
 * @see hook_notifications('event types')
 */
class Notifications_Event extends Notifications_Entity {
  // Object unique id
  public $eid;
  // Module and type of the event, will define it
  public $module = 'notifications';
  public $type = '';
  public $action = '';
  // Event key that will define event behavior
  public $typekey;
  // Main object id. I.e. if a node event it will be nid
  public $oid;
  // Time the event was produced
  public $created;
  public $triggered;
  // Notifications in queue linked to this event
  public $counter = 0;
  // Objects for this event
  public $objects = array();
  // Processing options, event to be queued
  public $queue = TRUE;
  // Processing options, notifications to be sent immediately
  public $send = TRUE;
  // Whether this has already been queued
  public $queued = FALSE;
  // Will be set if any of the objects cannot be loaded
  public $incomplete = FALSE;
  // Event text for composition
  public $text;
  // Template to use for this event
  protected $template;
  // Temporary cache, user access to event
  protected $_access;

  /**
   * Constructor
   */
  function __construct($object = NULL) {
    parent::__construct($object);
    if (!isset($this->created)) {
      $this->created = time();
    }
    if (!isset($this->typekey)) {
      $this->typekey = $this->type . '-' . $this->action;
    }
  }
  /**
   * Build Event from db object
   */
  public static function build_object($object) {
    $class = self::type_info($object->typekey, 'class', 'Notifications_Event');
    return new $class($object);
  }
  /**
   * Build Event from type and action
   */
  public static function build_type($type, $action) {
    $params = array('type' => $type, 'action' => $action);
    if ($class = self::type_info($type . '-' . $action, 'class')) {
      return new $class($params + self::type_info($type));
    }
    else {
      $class = self::type_info($type, 'class', 'Notifications_Event');
      // For this case $type and $action may override default type info 
      return new $class($params + self::type_info());
    }
  }
  /**
   * Get object title
   */
  public function get_title() {
    return t('Event');
  }
  /**
   * Get object name
   */
  public function get_name() {
    return t('Event');
  }
  /**
   * Get subscription type information
   */
  public static function type_info($type = NULL, $property = NULL, $default = NULL) {
    return notifications_event_type($type, $property, $default);
  }
  /**
   * Load event by id
   */
  public static function load($eid) {
    $event = entity_load('notifications_event', array($eid));
    return $event ? $event[$eid] : FALSE;
  }
  /**
   * Load multiple events
   */
  public static function load_multiple($eids = array(), $conditions = array()) {
    return entity_load('notifications_event', $eids, $conditions);
  }
  /**
   * Get event type information
   */
  function get_type($property = NULL) {
    if ($info = notifications_event_type($this->typekey)) {
      if ($property) {
        return isset($info[$property]) ? $info[$property] : NULL;
      }
      else {
        return $info;
      }
    }
  }
  /**
   * Get simple subject text
   */
  function get_subject() {
    return t('Notifications event');
  }
  
  /**
   * Get message template to build this event as text
   * 
   * The difference with create template is that this one keeps the template with the event so it can be reused
   */
  function get_template() {
    if (!isset($this->template)) {
      $this->template = $this->create_template();
    }
    return $this->template;
  }
  /**
   * Create message template to build this event as text
   * 
   * The value will be taken from this event's defaults, but can be overriden on hook_notifications('event types')
   */
  function create_template() {
    $template_name = $this->get_type('template', 'default');
    $class = notifications_info('message templates', $template_name, 'class', 'Notifications_Template');
    return new $class($this);
  }
  /**
   * Build template
   */
  function build_template() {
    return $this->get_template()->build();
  }
  
  /**
   * Get event text if available
   */
  function get_text($key) {
    if (isset($this->text[$key])) {
      return $this->text[$key];
    }
  }

  /**
   * Add Drupal Object, converting it into a Notifications_Object
   *
   * @param $object
   */
  function add_object($type, $value) {
    return $this->set_object(notifications_object($type, $value));
  }
  /**
   * Set Notifications object
   */
  function set_object($object) {
    $this->objects[$object->type] = $object;
    return $this;
  }
  /**
   * Get event objects
   */
  function get_objects() {
    return $this->objects;
  }
  /**
   * Get single object
   */
  function get_object($type) {
    return isset($this->objects[$type]) ? $this->objects[$type] : NULL;
  }
  /**
   * Check that all the objects still exist
   */
  function check_objects() {
    foreach ($this->get_objects() as $object) {
      if (!$object->value) return FALSE;
    }
    return TRUE;
  }

  /**
   * Trigger event. Save, run queue query, etc...
   * 
   * Replaces notifications_event_trigger($event)
   */
  function trigger() {
    // If already been triggered, do't do it again
    if (!empty($this->triggered)) {
      return FALSE;
    }
    /*
    $template = $this->get_template();
    dpm($template->build_message());
    */
    $this->send_all();
    // Notify other modules we are about to trigger some subscriptions event
    // Modules can do cleanup operations or modify event properties
    $this->triggered = REQUEST_TIME;
    $this->invoke_all('trigger');

    // Send event to queue for subscriptions, unless marked not to
    if ($this->queue) {
      return $this->queue();
    }
    elseif ($this->send) {
      return $this->send_all();
    }
    else {
      // Not for queue nor for sending, just discard
      return FALSE;
    }
  }

  /**
   * Create a record for the event and get unique eid
   */
  function record($update = FALSE) {
    if (!$this->eid || $update) {
      drupal_write_record('notifications_event', $this, $this->eid ? 'eid' : array());
    }
  }
  /**
   * Save full event
   */
  function save() {
    // First of all, make sure we have a unique eid
    $this->record();
    return db_update('notifications_event')
      ->condition('eid', $this->eid)
      ->fields(array('data' => serialize($this)))
      ->execute();
  }

  /**
   * Queue event for later processing
   */
  function queue() {
    // First of all, make sure we have a unique eid
    $this->queued = REQUEST_TIME;
    $this->record();
    // If advanced queue enabled, go for it. Otherwise, go for Drupal Queue
    if (function_exists('notifications_queue')) {
      $this->counter = notifications_queue()->queue_event($this);
    }
    else {
      $queue = DrupalQueue::get('notifications_queue');
      $queue->createItem($this); 
    }
    
    // Modules can do cleanup operations or modify the queue or the event counter
    $this->invoke_all('queued');

    // Now update event counter with rows in notifications_queue or delete if no rows
    if ($this->counter) {
      $this->update_counter();
    }
    else {
      $this->delete();
      return FALSE;
    }    
  }

  /**
   * Delete from db
   */
  function delete() {
    if (!empty($this->eid)) {
      // Inform all modules when we still have an eid, in case they have linked data
      $this->invoke_all('delete');
      // Finally, delete traces in our reference table
      return db_delete('notifications_event')->condition('eid', $this->eid)->execute();
      unset($this->eid);
    }
  }

  /**
   * Check user access to event's objects
   * 
   * Replaces notifications_event_user_access($event, $account);
   */
  public function user_access($account, $op = 'view') {
    foreach ($this->get_objects() as $object) {
      if (!$object->user_access($account)) {
        return FALSE;
      }
    }
    return TRUE;
  }
  
  /**
   * Set parameters
   */
  public function set_params($params = array()) {
    $params += array(
      'uid' => $GLOBALS['user']->uid,
      'language' => $GLOBALS['language']->language,
    );
    foreach ($params as $key => $value) {
      $this->$field = $value;
    }
    return $this;
  }
  
  /**
   * Clean up queued events
   * 
   * Replaces notifications_event_clean()
   * 
   * @param $update
   *   Update event counter
   */
  public static function queue_clean($update = FALSE) {
    return notifications_queue()->event_clean($update);
  }

  /**
   * Unserialize after db loading
   */
  public function unserialize() {
    $this->params = $this->params ? unserialize($this->params) : array();
  }
  /**
   * Track notifications queue row processed, decrease counter
   */
  function track_count() {
    return $this->counter ? --$this->counter : 0;
  }
  /**
   * Update event counter
   */
  function update_counter($value = NULL) {
    if (isset($value)) {
      $this->counter = $value;
    }
    db_query('UPDATE {notifications_event} SET counter = :counter WHERE eid = :eid', array(':counter' => $this->counter, ':eid' => $this->eid));
  }

  /**
   * Track events and keep them updated
   * 
   * It will delete events when objects are not available anymore or when event counter reaches 0
   * 
   * Replaces notifications_event_tracker()
   */

  /**
   * Add event to be tracked
   */
  public function track() {
    Notifications_Event_Tracker::track($this);
  }
  
  /**
   * Load and add to the tracker
   */ 
  public static function track_load($eid) {
    if (($event = self::load($eid)) && $event->load_objects() && empty($event->delete)) {
      $event->track_count();
      $event->track();
      return $event;
    }    
  }

  /**
   * Build query for subscriptions that match this event type
   */
  function query_subscriptions() {
    $query = db_select('notifications_subscription', 's');
    // Add field conditions for subscription types
    if ($types = $this->get_subscription_types()) {
      $condition = $this->get_field_conditions($types);
      if ($condition && $condition->count()) {
        $query
          ->leftJoin('notifications_subscription_fields', 'f', 's.sid = f.sid')
          ->condition($condition);
      }
      else {
        $query->condition('type', $types);
      }
    }
    return $query;
  }

  /**
   * Get field conditions
   */
  function get_field_conditions($subscription_types) {
    $add = db_or();
    foreach ($subscription_types as $type) {
      $condition = notifications_subscription_type($type)->event_conditions($this, db_or());
      if ($condition && $condition->count()) {
        $add->condition(
          db_and()
          ->condition('type', $this->type)
          ->condition($condition));         
      }
      else {
        $add->condition('type', $this->type);
      }
    }
    return $add;
  }
  /**
   * Get subscription types triggered by this event
   */
  function get_subscription_types() {
    return array_unique($this->invoke_all('subscription types'));
  }
  /**
   * Invoke all hook_notifications_event()
   */
  function invoke_all($op) {
    return module_invoke_all('notifications_event', $op, $this);
  }
  /**
   * Process events stored for immediate sending
   * 
   * This will be called from notifications_exit() when the event API is loaded
   */
  public static function process_immediate() {
    return Notifications_Event_Tracker::process_immediate();
  }
  
  /**
   * Send to all queued subscriptions
   */
  public function send_all() {
    $sids = $this->query_subscriptions()
    ->fields('s', array('sid'))
    ->execute()
    ->fetchCol();
    if ($sids) {
      $subscriptions = entity_load('notifications_subscription', $sids);
      return $this->send_list($subscriptions);
    }
  }
  /**
   * Send to subscriptors
   * 
   * @param $subscriptions
   *   List of subscriptions to send to
   */
  public function send_list($subscriptions) {
    // Template to be rendered for each user
    $template = $this->get_template();
    // Keep track of users sent so we don't repeat
    $sent = $results = array();
    foreach ($subscriptions as $subscription) {
      $user = $subscription->get_user();
      if (in_array($user->uid, $sent)) continue;
      if ($this->user_access($user)) {
        $template->add_object('user', $user);     
        $message = $template->build_message()
          ->set_method($subscription->send_method);
        $message->set_user($user);
        $results[] = $result = $message->send();
        if ($result) {
          $sent[] = $user->uid;
        }
      }
      else {
        // User doesn't have access, mark as sent so we don't try again
        $sent[] = $user->uid;
        $results[] = FALSE;
      }
    }
    $count = count($results);
    $success = count(array_filter($results));
    watchdog('notifications', 'Sent @count notifications: @success success, @errors errors', array('@count' => $count, '@success' => $success, '@errors' => $count - $success));
    return $success;
  }
}

/**
 * Event tracker
 */
class Notifications_Event_Tracker {
  // Track events for immediate sending
  protected static $_send_immediate;
  // Track events for queue processing
  protected static $_event_tracker;

  /**
   * Add event to be tracked
   */
  public function track($event) {
    self::$_event_tracker[$event->eid] = $event;
  }

  // Delete event and all related rows
  public static function delete($event) {
    unset(self::$_event_tracker[$event->eid]);
    $event->delete();
  }

  // Update tracked events counter or delete if counter reached zero or marked for deletion.
  public static function update() {
    if (!empty(self::$_event_tracker)) {
      foreach (self::$_event_tracker as $eid => $event) {
        if (!$event->counter || $event->incomplete || !empty($event->delete)) {
          $event->delete();
        }
        else {
          $event->update_counter();
        }
        unset(self::$_event_tracker[$eid]);
      }
    }
  }
  static function reset() {
    self::$_event_tracker = array();    
  }
  /**
   * Store / return events for immediate sending
   */
  public static function send_immediate($event = NULL) {
    if ($event) {
      // Store events anyway so they can be accessed through the static variable
      // and other modules can have a peek on what has happened on this request
      self::$_send_immediate[$event->eid] = $event;
    }
    elseif (!empty(self::$_send_immediate)) {
      return self::$_send_immediate;
    }
  }
  
  /**
   * Process events stored for immediate sending
   * 
   * This will be called from notifications_exit() when the event API is loaded
   */
  public static function process_immediate() {
    if ($events = self::send_immediate()) {
      foreach ($events as $event) {
        notifications_queue()->process_rows(array('cron' => 1, 'eid' => $event->eid, 'send_interval' => 0));
      }    
    }
  }

}