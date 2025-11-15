#include "XmlRpc.h"
#include <iostream>

int main(int argc, char** argv) {
  const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
  int port = (argc > 2) ? std::atoi(argv[2]) : 8080;
  std::string user = (argc > 3) ? argv[3] : "admin";
  std::string pass = (argc > 4) ? argv[4] : "1234";

  XmlRpc::XmlRpcClient client(host, port, "/RPC2");
  XmlRpc::XmlRpcValue params, result;
  params["username"] = user;
  params["password"] = pass;

  bool ok = client.execute("auth.login", params, result);
  if (!ok) {
    std::cerr << "Llamada RPC falló a nivel transporte.\n";
    return 2;
  }

  std::cout << "Respuesta XML-RPC:\n" << result.toXml() << "\n";

  if (result.hasMember("status") && int(result["status"]["code"]) == 0) {
    std::string token = std::string(result["payload"]["token"]);
    std::cout << "Login OK. Token = " << token << "\n";
    return 0;
  } else {
    int code = result.hasMember("status") ? int(result["status"]["code"]) : -1;
    std::string msg = result.hasMember("status") ? std::string(result["status"]["msg"]) : "NO_STATUS";
    std::cerr << "Login FAIL. code=" << code << " msg=" << msg << "\n";
    return 3;
  }
}
