// Copyright (C) 2013 Robert Giseburt <giseburt@gmail.com>
// serialport_poller.cpp Written as a part of https://github.com/voodootikigod/node-serialport
// License to use this is the same as that of node-serialport.

#include <nan.h>
#include "./serialport_poller.h"
#include <poll.h>

using namespace v8;

static Nan::Persistent<v8::FunctionTemplate> serialportpoller_constructor;

SerialportPoller::SerialportPoller() :  Nan::ObjectWrap() {}
SerialportPoller::~SerialportPoller() {
  delete callback_;
  delete logger_callback;
}

void SerialportPoller::_serialportReadable(uv_poll_t *req, int status, int events) {
  Nan::HandleScope scope;
  SerialportPoller* sp = (SerialportPoller*) req->data;
  // We can stop polling until we have read all of the data...
  sp->_stop();

  v8::Local<v8::Value> argv[1];
  snprintf(sp->errorString, sizeof(sp->errorString), "Got some bytes to read from fd %i", sp->fd_);
  argv[0] = Nan::New<v8::String>(sp->errorString).ToLocalChecked();
  sp->logger_callback->Call(1, argv);
  sp->callCallback(status);
}

void SerialportPoller::_serialportTimeout(uv_timer_t *req) {
  Nan::HandleScope scope;
  SerialportPoller* sp = (SerialportPoller*) req->data;
  // We can stop polling until we have read all of the data...
  sp->_stop();

  // Check we have a data to read
  pollfd pfd = { .fd = sp->fd_, .events = POLLIN };
  int express_poll_result = poll(&pfd, 1, 0); // We want result immediately.
  if (express_poll_result != 1) {
	  sp->_start();
	  return;
  }

  v8::Local<v8::Value> argv[1];
  if (pfd.revents & POLLIN) {
	  snprintf(sp->errorString, sizeof(sp->errorString), "Got some bytes to read from fd %i by timeout", sp->fd_);
	  argv[0] = Nan::New<v8::String>(sp->errorString).ToLocalChecked();
	  sp->logger_callback->Call(1, argv);
	  sp->callCallback(0);
  } else {
	  snprintf(sp->errorString, sizeof(sp->errorString), "Proceed with polling fd %i after timeout", sp->fd_);
	  argv[0] = Nan::New<v8::String>(sp->errorString).ToLocalChecked();
	  sp->logger_callback->Call(1, argv);
	  sp->_start();
  }
}

void SerialportPoller::callCallback(int status) {
  Nan::HandleScope scope;
  // uv_work_t* req = new uv_work_t;

  // Call the callback to go read more data...

  v8::Local<v8::Value> argv[1];
  if (status != 0) {
    // error handling changed in libuv, see:
    // https://github.com/joyent/libuv/commit/3ee4d3f183331
    #ifdef UV_ERRNO_H_
    const char* err_string = uv_strerror(status);
    #else
    uv_err_t errno = uv_last_error(uv_default_loop());
    const char* err_string = uv_strerror(errno);
    #endif
    snprintf(this->errorString, sizeof(this->errorString), "Error %s on polling", err_string);
    argv[0] = v8::Exception::Error(Nan::New<v8::String>(this->errorString).ToLocalChecked());
  } else {
    argv[0] = Nan::Undefined();
  }

  callback_->Call(1, argv);
}



void SerialportPoller::Init(Handle<Object> target) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New<String>("SerialportPoller").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);


  // Prototype

  // SerialportPoller.close()
  Nan::SetPrototypeMethod(tpl, "close", Close);

  // SerialportPoller.start()
  Nan::SetPrototypeMethod(tpl, "start", Start);

  Nan::SetPrototypeMethod(tpl, "SetLoggerCallback", SetLoggerCallback);

  serialportpoller_constructor.Reset(tpl);

  Nan::Set(target, Nan::New<String>("SerialportPoller").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(SerialportPoller::New) {
  if (!info[0]->IsInt32()) {
    Nan::ThrowTypeError("First argument must be an fd");
    return;
  }

  if (!info[1]->IsFunction()) {
    Nan::ThrowTypeError("Third argument must be a function");
    return;
  }

  SerialportPoller* obj = new SerialportPoller();
  obj->fd_ = info[0]->ToInt32()->Int32Value();
  obj->callback_ = new Nan::Callback(info[1].As<v8::Function>());
  // obj->callCallback();

  obj->Wrap(info.This());

  obj->poll_handle_.data = obj;
  obj->poll_timer.data = obj;

  uv_poll_init(uv_default_loop(), &obj->poll_handle_, obj->fd_);
  uv_timer_init(uv_default_loop(), &obj->poll_timer);

  uv_poll_start(&obj->poll_handle_, UV_READABLE, _serialportReadable);
  uv_timer_start(&obj->poll_timer, _serialportTimeout, poll_timeout, 0);

  info.GetReturnValue().Set(info.This());
}


NAN_METHOD(SerialportPoller::SetLoggerCallback) {
  if (!info[0]->IsFunction()) {
    Nan::ThrowTypeError("First argument must be a function");
    return;
  }
  SerialportPoller* obj = Nan::ObjectWrap::Unwrap<SerialportPoller>(info.This());
  obj->logger_callback = new Nan::Callback(info[0].As<v8::Function>());
}

void SerialportPoller::_start() {
  v8::Local<v8::Value> argv[1];
  snprintf(this->errorString, sizeof(this->errorString), "Start polling serial port fd %i", this->fd_);
  argv[0] = Nan::New<v8::String>(this->errorString).ToLocalChecked();
  logger_callback->Call(1, argv);
  uv_poll_start(&poll_handle_, UV_READABLE, _serialportReadable);
  uv_timer_start(&poll_timer, _serialportTimeout, poll_timeout, 0);
}

void SerialportPoller::_stop() {
  v8::Local<v8::Value> argv[1];
  snprintf(this->errorString, sizeof(this->errorString), "Stop polling serial port fd %i", this->fd_);
  argv[0] = Nan::New<v8::String>(this->errorString).ToLocalChecked();
  logger_callback->Call(1, argv);
  uv_poll_stop(&poll_handle_);
  uv_timer_stop(&poll_timer);
}


NAN_METHOD(SerialportPoller::Start) {
  SerialportPoller* obj = Nan::ObjectWrap::Unwrap<SerialportPoller>(info.This());
  obj->_start();

  return;
}

NAN_METHOD(SerialportPoller::Close) {
  SerialportPoller* obj = Nan::ObjectWrap::Unwrap<SerialportPoller>(info.This());
  obj->_stop();

  // DO SOMETHING!

  return;
}
