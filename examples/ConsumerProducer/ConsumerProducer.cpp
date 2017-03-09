// Dogee.cpp : 定义控制台应用程序的入口点。
//
#ifdef _WIN32
#include "stdafx.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "DogeeBase.h"
#include "DogeeMacro.h"
#include "DogeeRemote.h"
#include "DogeeThreading.h"
#include "DogeeHelper.h"
#include <memory>
using namespace Dogee;

template <typename T>
class ProducerConsumerQueue : public DObject
{
	DefBegin(DObject);
	Def(have_item, Ref<DSemaphore>);
	Def(have_place, Ref<DSemaphore>);
	Def(mutex, Ref<DSemaphore>);
	Def(size, int);
	Def(head, int);
	Def(tail, int);
	Def(buf, Array<T>);
public:
	DefEnd();
	ProducerConsumerQueue(ObjectKey obj_id) : DObject(obj_id)
	{
	}
	ProducerConsumerQueue(ObjectKey obj_id, int size) : DObject(obj_id)
	{
		self->size = size;
		self->head = 0;
		self->tail = 0;
		self->have_item = NewObj<DSemaphore>(0);
		self->have_place = NewObj<DSemaphore>(size);
		self->mutex = NewObj<DSemaphore>(1);
		self->buf = NewArray<T>(size);
	}

	void Produce(T val)
	{
		self->have_place->Acquire();
		self->mutex->Acquire();
		self->buf[tail] = val;
		self->tail = (self->tail + 1) % self->size;
		self->mutex->Release();
		self->have_item->Release();
	}
	T Consume()
	{
		self->have_item->Acquire();
		self->mutex->Acquire();
		T ret = self->buf[head];
		self->head = (self->head + 1) % self->size;
		self->mutex->Release();
		self->have_place->Release();
		return ret;
	}
	void Destory()
	{
		DelObj((Ref<DSemaphore>)self->have_item);
		DelObj((Ref<DSemaphore>)self->have_place);
		DelObj((Ref<DSemaphore>)self->mutex); 
		DelArray((Array<T>)self->have_place);
	}
};

class Node : public DObject
{
	DefBegin(DObject);
public:
	Def(size, int);
	Def(data, int);
	Def(children, Array<Ref<Node>>);
	Node(ObjectKey obj_id) : DObject(obj_id)
	{
	}
};

DefGlobal(queue,Ref < ProducerConsumerQueue<Ref<Node>>>);
void WorkingThread(uint32_t param)
{
	for (;;)
	{
		Ref<Node> node = queue->Consume();
		for (int i = 0; i < node->size; i++)
		{
			queue->Produce(node->children[i]);
		}
	}
}
RegFunc(WorkingThread);



int main(int argc, char* argv[])
{
	HelperInitCluster(argc, argv);

	return 0;
}
