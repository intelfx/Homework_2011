#include "stdafx.h"
#include "LinkedList.h"
#include "DoubleLinkedList.h"

#include <uXray/fxlog_console.h>

// -----------------------------------------------------------------------------
// Library		Homework
// File			ListDemo.cpp
// Author		intelfx
// Description	List demonstration and unit-tests.
// -----------------------------------------------------------------------------

int main (int, char**)
{
	Debug::System::Instance().SetTargetProperties (Debug::CreateTarget ("stderr", EVERYTHING, EVERYTHING),
												   &FXConLog::Instance());

	LinkedList<int> l;
	l.PushFront (1);
	l.PushFront (2);
    l.PushFront (3);

    l.PushFront (100500);
    smsg (E_INFO, E_USER, "Popped: %d", l.PopFront());

    l.PushFront (10);
    l.PushFront (20);
    l.PushFront (30);

    for (auto i = l.Begin(); !i.End(); ++i)
    {
        smsg (E_INFO, E_USER, "Single-linked list: Data: %d", *i);
    }

	l.PopFront();

	DoubleLinkedList<int> dl;

	dl.PushFront (1);
	dl.PushFront (2);
	dl.PushFront (3);

	dl.PushBack (100500);
	smsg (E_INFO, E_USER, "Popped: %d", dl.PopBack());

	dl.PushBack (-1);
	dl.PushBack (-2);
	dl.PushBack (-3);

	for (auto i = dl.Begin(); !i.End(); ++i)
	{
		smsg (E_INFO, E_USER, "Double-linked list: Forward iteration: Data: %d", *i);
	}

	dl.PopFront();
	dl.PopBack();

	Debug::System::Instance.ForceDelete();
}
