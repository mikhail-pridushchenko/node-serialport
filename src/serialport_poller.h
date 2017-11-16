// Copyright (C) 2013 Robert Giseburt <giseburt@gmail.com>
// serialport_poller.h Written as a part of https://github.com/voodootikigod/node-serialport
// License to use this is the same as that of node-serialport.

#ifndef SERIALPORT_POLLER_H
#define SERIALPORT_POLLER_H

#include <nan.h>
#include "./serialport.h"

class SerialportPoller : public Nan::ObjectWrap {
 public:
  static void Init(v8::Handle<v8::Object> target);

  void callCallback(int status);
  static NAN_METHOD(SetLoggerCallback);

  void _start();
  void _stop();

  static const uint64_t poll_timeout = 15000; // 15 seconds
 private:
  SerialportPoller();
  ~SerialportPoller();

  static NAN_METHOD(New);
  static NAN_METHOD(Close);
  static NAN_METHOD(Start);

  static void _serialportReadable(uv_poll_t*, int, int);
  static void _serialportTimeout(uv_timer_t*);

  uv_poll_t poll_handle_;
  uv_timer_t poll_timer;
  int fd_;
  char errorString[ERROR_STRING_SIZE];

  Nan::Callback* callback_;
  Nan::Callback* logger_callback;
};

#endif
