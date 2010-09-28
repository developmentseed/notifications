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
class Notifications_Schedule_Event extends Notifications_Event {
  // Default system queue name and item if came from queue
  public $queue = 'notifications_event';
  protected $queue_item;
  // Content that needs to be digested in this event.
  // It can be nodes, comments, etc...
  protected $content;
  // Subscriptions counter, this starts with 1 (si the event record is not deleted)
  // And with every process is set to the max sid processed
  /**
   * Trigger this event
   */
  function trigger() {
    // Always queue for later processing
    $this->queue();  
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