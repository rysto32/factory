// Copyright (c) 2018 Ryan Stone.  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

#include "EventLoop.h"

#include "Event.h"

#include <stdexcept>
#include <stdio.h>
#include <event.h>

EventLoop::EventLoop()
{
	ev_base = event_base_new();
	if (ev_base == NULL)
		throw std::runtime_error("event_base_new() failed");
}

EventLoop::~EventLoop()
{
	event_base_free(ev_base);
}

void
EventLoop::RegisterSignal(Event * event, int sig)
{
	struct event *ev;

	ev = event_new(ev_base, sig, EV_SIGNAL | EV_PERSIST, EventCallback, event);
	if (ev == NULL)
		throw std::runtime_error("event_new() failed");

	event->SetEvent(ev);
	event_add(ev, NULL);
}

void
EventLoop::RegisterListenSocket(Event *event, int fd)
{
	struct event *ev;

	ev = event_new(ev_base, fd, EV_READ | EV_PERSIST, EventCallback, event);
	if (ev == NULL)
		throw std::runtime_error("event_new() failed");

	event->SetEvent(ev);
	event_add(ev, NULL);
}

void
EventLoop::RegisterSocket(Event *event, int fd)
{
	struct event *ev;

	ev = event_new(ev_base, fd, EV_CLOSED | EV_READ | EV_PERSIST, EventCallback, event);
	if (ev == NULL)
		throw std::runtime_error("event_new() failed");

	event->SetEvent(ev);
	event_add(ev, NULL);
}

void
EventLoop::EventCallback(evutil_socket_t fd, short flags, void *arg)
{
	Event * event = static_cast<Event*>(arg);

	event->Dispatch(fd, flags);
}

void
EventLoop::Run()
{
	event_base_dispatch(ev_base);
}

void
EventLoop::SignalExit()
{
	event_base_loopbreak(ev_base);
}
