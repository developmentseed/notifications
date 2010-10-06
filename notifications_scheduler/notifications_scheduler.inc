<?php
// $Id: notifications_event.class.inc,v 1.1.2.13 2010/07/22 11:57:49 jareyero Exp $
/**
 * @file
 * Drupal Notifications Framework - Default class file
 */


/**
 * Notifications Schedule Event class
 * 
 * Unlike regular events, these are full auto-contained events that know which message to produce
 * and how to send it. Thus, these events will be queued in regular Drupal queue
 */
abstract class Notifications_Scheduler_Event extends Notifications_Event {
  // Default system queue name and item if came from queue
  public $queue = 'notifications_event';
  protected $queue_item;
  // Subscriptions counter, this starts with 1 (si the event record is not deleted)
  // And with every process is set to the max sid processed
  public $counter = 1;
  // Content that needs to be digested in this event.
  // It can be nodes, comments, etc...
  protected $content;
  // Pre-built template, so it can be reused for every sending
  protected $template;
  // Subscriptions to be processed on every batch, indexed by sid
  protected $subscriptions;
  /**
   * Trigger this event
   */
  function trigger() {
    // Load the content now, later it may be different
    if ($this->content = $this->load_content()) {
      // Build message template so it can be reused later
      if ($this->template = $this->build_template()) {
        // Always queue for later processing
        $this->queue();
        return TRUE;
      }
    }
    // If something failed we don't even store this event
    return FALSE;
  }
  /**
   * Load content, to be implemented by subclasses
   */
  public abstract function load_content();

  /**
   * Build template, to be implemented by subsclasses
   */
  public abstract function build_template();

  /**
   * Process event, send pending notifications. Subscriptions start on $counter (min sid)
   * 
   * @param $limit
   *   Maximum number of subscriptions to process
   */
  function process($limit = 10) {
    while ($limit && ($subscriptions = $this->get_subscriptions($limit))) {
      $limit = $limit - count($subscriptions);
      // New value for the counter if this goes right
      $counter = max(array_keys($subscriptions));
      // Turn subscriptions into batches, groups, etc...
      $groups = $this->prepare_subscriptions($subscriptions);
      // Now process groups one at a time
      foreach ($groups as $group) {
        $results = $this->process_group($groups);
      }
      // Update counter on the event record
      $this->update_counter($counter);
    }
    if (!empty($counter)) {
      // We will do more processing later
      $this->release();
    }
    else {
      // Nothing to process, delete all this
      $this->delete();
    }
  }
  /**
   * Get subscriptions that match this event criteria
   * 
   * This should return an array of Notifications_Qeue_Item's
   */
  protected function get_subscriptions() {
    
  }
  /**
   * Groups subscriptions. This one will just create a group for all
   */
  protected function prepare_subscriptions($subscriptions) {
    return array($subscriptions);
  }
  /**
   * Process group, add all to a message and send out
   */
  protected function process_group($group) {
    $result = array('messages' => 0, 'items' => 0, 'errors' => 0);
    $message = $this->build_message();
    foreach ($group as $item) {
      if ($destination = $item->get_destination()) {
        $message->add_destination($destination);
      }
      else {
        $result['errors']++;
      }
      $result['items']++;
    }
    if ($message->send()) {
      $result['messages']++;
    }
    return $result;
  }
  /**
   * Build message from the template we have stored
   * 
   * @return Messaging_Message object
   */
  function build_message() {
    return $this->template->build_message();
  }
  /**
   * Queue event for later processing
   */
  function queue() {
    // First of all, make sure we have a unique eid
    $this->record();
    // Now store in system queue for later processing
    $this->queued = TRUE;
    $this->get_queue()->createItem($this);
  }

  /**
   * Delete from queue and everywhere else
   */
  function delete() {
    // If coming from a queued item, delete it too
    if (isset($this->queue_item)) {
      $this->get_queue()->deleteItem($this->queue_item);
      unset($this->queue_item);
    }
    parent::delete();
  }
  /**
   * Release if process not finished
   */
  function release() {
    if (!isset($this->queue_item)) {
      $this->get_queue()->releaseItem($this->queue_item);
      unset($this->queue_item);
    }    
  }
  
  /**
   * Build from queued item
   */
  public static function build_from_queue($item) {
    $event = $item->data;
    $event->queue_item = $item;
    return $event;
  }

  /**
   * Get system queue for operations
   */
  function get_queue() {
    return Drupal_Queue::get($this->queue);
  }
}

/**
 * Test this schedule class, send latest created nodes
 */
class Notifications_Scheduler_Latest extends Notifications_Scheduler_Event {
  public $node_type = 'story';
  
  /**
   * Load latest nodes created. It doesn't check node access
   */
  protected function load_content() {
    $select = db_select('node', 'n')
      ->fields('n', array('nid', 'created'))
      ->condition('status', 1)
      ->orderBy('created', 'DESC')
      ->limit(variable_get('default_nodes_main', 10))
    $nids = $select->execute()->fetchCol();
    return node_load_multiple($nids);
  }
  /**
   * Build message template. It doesn't check node access
   */
  protected function build_template() {
    $template = $this->get_template('message');
    $template->add_string('subject', $this->get_subject());
    $body = $template->add_template('body', $this->get_template('body'));
    $body->add_item('header', $this->get_header());
    $content = $body->add_element('content', $this->get_template('content'));
    $body->add_item('footer', $this->get_footer());
    // All nodes will be stored, the template will possibly render them with node_view_multiple()
    foreach ($this->content as $nid => $node) {
      $content->add_item($nid, $node);
    }
    return $template;
  }
  
}