#include "app/RPCAuthLogin.h"
#include <chrono>

using namespace XmlRpc;

RpcAuthLogin::RpcAuthLogin(XmlRpcServer* srv, app::AppServer& app)
  : XmlRpcServerMethod("auth.login", srv), app_(app) {}

void RpcAuthLogin::execute(XmlRpcValue& params, XmlRpcValue& result) {
  // Esperamos un struct con claves: username, password
  if (params.getType() != XmlRpcValue::TypeArray || params.size() != 1 ||
      params[0].getType() != XmlRpcValue::TypeStruct ||
      !params[0].hasMember("username") || !params[0].hasMember("password")) {
    result["status"]["code"] = 400;
    result["status"]["msg"]  = "BAD_REQUEST";
    result["payload"] = ""; // sin payload
    return;
  }

  XmlRpcValue& req = params[0];
  std::string username = std::string(req["username"]);
  std::string password = std::string(req["password"]);
  std::string clientIp = req.hasMember("client_ip") ? std::string(req["client_ip"]) : "";
  std::string userAgent = req.hasMember("user_agent") ? std::string(req["user_agent"]) : "";

  auto lr = app_.auth().login(username, password, clientIp, userAgent);

  result["status"]["code"] = static_cast<int>(lr.status);
  result["status"]["msg"]  = lr.message;
  if (lr.status == app::AuthStatus::Ok) {
    result["payload"]["token"] = lr.token;
    if (lr.expiresAt.time_since_epoch().count() != 0) {
      auto expires = std::chrono::duration_cast<std::chrono::seconds>(
          lr.expiresAt.time_since_epoch()).count();
      result["payload"]["expires_at"] = static_cast<double>(expires);
    }
    if (lr.user) {
      result["payload"]["privilege"] = lr.user->privilege;
      result["payload"]["username"] = lr.user->username;
    }
  } else {
    result["payload"]["token"] = "";
    if (lr.retryAfterSeconds > 0) {
      result["payload"]["retry_after"] = lr.retryAfterSeconds;
    }
  }
}
