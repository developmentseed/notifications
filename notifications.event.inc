<?php
// $Id: notifications_event.class.inc,v 1.1.2.13 2010/07/22 11:57:49 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */


/**
 * Notifications Event class
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
  // Main object id
  public $oid;
  // Time the event was produced
  public $created;
  // Notifications in queue linked to this event
  public $counter = 0;
  // Objects for this event
  public $objects = array();
  // Processing options, not saved
  public $save = TRUE;
  public $queue = TRUE;
  public $queued = FALSE;
  // Will be set if any of the objects cannot be loaded
  public $incomplete = FALSE;
  // Event text for composition
  public $text;
  // Template to use for this event
  protected $template;
  // Temporary storage for digesting information
  public $digest;
  // Temporary cache, user access to event
  protected $_access;
  // Shared template map for all events
  protected static $_templates;

  /**
   * Constructor
   */
  function __construct($template = NULL) {
    parent::__construct($template);
    if (!isset($this->created)) {
      $this->created = time();
    }
    if (!isset($this->typekey)) {
      $this->typekey = $this->type . '-' . $this->action;
    }
  }
  /**
   * Build Event Object
   */
  public static function build_object($object) {
    $class = notifications_event_type($object->type, 'class', 'Notifications_Event');
    return new $class($object);
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
    if ($info = notifications_event_types($this->typekey)) {
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
   * The value will be taken from this event's defaults, but can be overriden on hook_notifications('event types')
   * 
   * @param $type
   *   Template type: 'message', 'line', 'content', etc...
   */
  function get_template($type) {
    $defaults = $this->default_templates();
    $class = $this->get_type($type . '_template', $defaults[$type]);
    return new $class($this);
  }

  /**
   * Build template
   */
  function build_template() {
    if ($template = self::template_map($this->typekey)) {
      $this->template = $template;
    }
    else {
      // Build default that will be like notifications-event-type-action
      $parts = array($this->module, 'event', $this->type, $this->action);
      $this->template = implode('-', array_filter($parts));
    }   
  }
  
  /**
   * Get event text if available
   */
  function get_text($key) {
    if (isset($this->text[$key])) {
      return $this->text[$key];
    }
    elseif (isset($this->params['text'][$key])) {
      return $this->params['text'][$key];
    }
  }

  /**
   * Get set event object
   */
  function object($type, $object = NULL) {
    if (isset($object)) {
      $this->add_object($type, $object);
    }
    else {
      $this->load_objects();
      return isset($this->objects[$type]) ? $this->objects[$type] : NULL;
    }
  }
  /**
   * Add Notifications Object
   *
   * @param $object Notifications_Object
   */
  function add_object($object) {
    $this->objects[$object->type] = $object;
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
    // Notify other modules we are about to trigger some subscriptions event
    // Modules can do cleanup operations or modify event properties
    $this->invoke_all('trigger'); 
    // Store event, unles marked not to be saved 
    if ($this->save) {    
      $this->save();
    }
    // Send event to queue for subscriptions, unless marked not to
    if ($this->queue) {
      $this->queue();
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
    return $this->db_update('notifications_event')
      ->condition('eid', $this->eid)
      ->fiels(array('data' => serialize($this)));
  }

  /**
   * Queue event for later processing
   */
  function queue() {
    // First of all, make sure we have a unique eid
    $this->record();
    $this->queued = TRUE;
    $this->counter = notifications_queue()->queue_event($this);

    // Modules can do cleanup operations or modify the queue or the event counter
    $this->invoke_all('queued');

    // Now update event counter with rows in notifications_queue or delete if no rows
    if ($this->counter) {
      $this->update_counter();
      // If immediate sending enabled, store it for sending on page exit.
      if (variable_get('notifications_send_immediate', 0)) {
        Notifications_Event_Tracker::send_immediate($this);
      }
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
   * Build new event object from array of data
   * 
   * Replaces notifications_event_build()
   * 
   * @param $params
   *   Event properties
   * @param $objects
   *   Array of event objects (type => $object)
   */
  public static function create($params, $objects = array()) {
    global $user, $language;
    
    // Fill in event with default values
    $params += array(
      'uid' => $user->uid,
      'language' => $language->language,
      'type' => 'default', // Object/event type
      'action' => 'default', // Action that happened to the object
    );
    $params += array('typekey' => $params['type'] . '-' . $params['action']);
    // Check whether we have to save and queue this event, defaults to yes if not set
    // If not enabled, do not store nor queue this event, can be changed by plug-in modules
    $enabled = notifications_event_enabled($params['typekey']);
    $params += array('save' => $enabled, 'queue' => $enabled);
    $event = new Notifications_Event($params);
    foreach ($objects as $type => $object) {
      $event->add_object($type, $object);
    }
    // Modules can do cleanup operations or modify event properties
    $event->invoke_all('build');
    return $event;
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
   * Get default templates for this event
   */
  function default_templates() {
    return array(
      'message' => 'Notifications_Message_Template',
      'body' => 'Notifications_Message_Body',
      'content' => 'Notifications_Message_Content',
      
    );
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
          ->leftJoin('notifications_fields', 'f', 's.sid = f.sid')
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
    return module_invoke_all($op, $this);
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