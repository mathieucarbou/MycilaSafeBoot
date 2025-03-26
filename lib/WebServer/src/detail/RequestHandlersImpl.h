#ifndef REQUESTHANDLERSIMPL_H
#define REQUESTHANDLERSIMPL_H

#include "RequestHandler.h"
#include "WString.h"
#include "Uri.h"
#include <MD5Builder.h>
#include <base64.h>

bool RequestHandler::process(WebServer &server, HTTPMethod requestMethod, String requestUri) {
  if (_chain) {
    return _chain->runChain(server, [this, &server, &requestMethod, &requestUri]() {
      return handle(server, requestMethod, requestUri);
    });
  } else {
    return handle(server, requestMethod, requestUri);
  }
}

class FunctionRequestHandler : public RequestHandler {
public:
  FunctionRequestHandler(WebServer::THandlerFunction fn, WebServer::THandlerFunction ufn, const Uri &uri, HTTPMethod method)
    : _fn(fn), _ufn(ufn), _uri(uri.clone()), _method(method) {
    _uri->initPathArgs(pathArgs);
  }

  ~FunctionRequestHandler() {
    delete _uri;
  }

  bool canHandle(HTTPMethod requestMethod, const String &requestUri) override {
    if (_method != HTTP_ANY && _method != requestMethod) {
      return false;
    }

    return _uri->canHandle(requestUri, pathArgs);
  }

  bool canUpload(const String &requestUri) override {
    if (!_ufn || !canHandle(HTTP_POST, requestUri)) {
      return false;
    }

    return true;
  }

  bool canRaw(const String &requestUri) override {
    if (!_ufn || _method == HTTP_GET) {
      return false;
    }

    return true;
  }

  bool canHandle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
    if (_method != HTTP_ANY && _method != requestMethod) {
      return false;
    }

    return _uri->canHandle(requestUri, pathArgs) && (_filter != NULL ? _filter(server) : true);
  }

  bool canUpload(WebServer &server, const String &requestUri) override {
    if (!_ufn || !canHandle(server, HTTP_POST, requestUri)) {
      return false;
    }

    return true;
  }

  bool canRaw(WebServer &server, const String &requestUri) override {
    if (!_ufn || _method == HTTP_GET || (_filter != NULL ? _filter(server) == false : false)) {
      return false;
    }

    return true;
  }

  bool handle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
    if (!canHandle(server, requestMethod, requestUri)) {
      return false;
    }

    _fn();
    return true;
  }

  void upload(WebServer &server, const String &requestUri, HTTPUpload &upload) override {
    (void)upload;
    if (canUpload(server, requestUri)) {
      _ufn();
    }
  }

  void raw(WebServer &server, const String &requestUri, HTTPRaw &raw) override {
    (void)raw;
    if (canRaw(server, requestUri)) {
      _ufn();
    }
  }

  FunctionRequestHandler &setFilter(WebServer::FilterFunction filter) {
    _filter = filter;
    return *this;
  }

protected:
  WebServer::THandlerFunction _fn;
  WebServer::THandlerFunction _ufn;
  // _filter should return 'true' when the request should be handled
  // and 'false' when the request should be ignored
  WebServer::FilterFunction _filter;
  Uri *_uri;
  HTTPMethod _method;
};

#endif  //REQUESTHANDLERSIMPL_H
