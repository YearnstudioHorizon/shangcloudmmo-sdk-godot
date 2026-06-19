#include "mmo_message_queue.h"

MMOMessageQueue::MMOMessageQueue() {
	mutex.instantiate();
}

void MMOMessageQueue::push(const MMOMessage &p_msg) {
	mutex->lock();
	queue.push_back(p_msg);
	mutex->unlock();
}

Vector<MMOMessage> MMOMessageQueue::drain() {
	mutex->lock();
	Vector<MMOMessage> result = queue;
	queue.clear();
	mutex->unlock();
	return result;
}

bool MMOMessageQueue::is_empty() {
	mutex->lock();
	bool empty = queue.is_empty();
	mutex->unlock();
	return empty;
}
