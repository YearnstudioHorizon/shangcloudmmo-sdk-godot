#ifndef MMO_MESSAGE_QUEUE_H
#define MMO_MESSAGE_QUEUE_H

#include <godot_cpp/classes/mutex.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

struct MMOMessage {
	enum Type {
		TEXT,
		BINARY,
	};

	Type type;
	String text;
	PackedByteArray data;
};

class MMOMessageQueue {
public:
	MMOMessageQueue();

	void push(const MMOMessage &p_msg);
	Vector<MMOMessage> drain();
	bool is_empty();

private:
	Ref<Mutex> mutex;
	Vector<MMOMessage> queue;
};

#endif
