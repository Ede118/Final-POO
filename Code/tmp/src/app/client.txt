/* client.cpp : codigo auxiliar para TP de un cliente XMLRPC con bucle.
   Uso: client Host Port
*/
#include <iostream>
#include <stdlib.h>
using namespace std;

#include "XmlRpc.h"
using namespace XmlRpc;

int main(int argc, char* argv[])
{
  if (argc != 3) {
    std::cerr << "Uso: miclient IP_HOST N_PORT\n";
    return -1;
  }
  
  int port = atoi(argv[2]);
  //XmlRpc::setVerbosity(5);

  XmlRpcClient c(argv[1], port);

  int cont = 0;
  XmlRpcValue noArgs, result;
  XmlRpcValue oneArg;
  XmlRpcValue numbers;

  while(true){
    switch(cont){
      case 0:
        std::cout << "---------------------------------------------------------------------" << std::endl ;
        std::cout << "*********** Una mirada a los metodos soportados por la API **********" << std::endl ;
        if (c.execute("system.listMethods", noArgs, result))
          std::cout << "\nMetodos:\n " << result << "\n\n";
        else
          std::cout << "Error en la llamada a 'listMethods'\n\n";
        break;
      case 1:
        std::cout << "---------------------------------------------------------------------" << std::endl ;
        std::cout << "****** Peticion para recuperar una ayuda sobre el metodo Eco ******" << std::endl ;
        oneArg[0] = "Eco";
        if (c.execute("system.methodHelp", oneArg, result))
          std::cout << "Ayuda para el metodo 'Eco': " << result << "\n\n";
        else
          std::cout << "Error en la llamada a 'methodHelp'\n\n";
        break;
      case 2:
        std::cout << "---------------------------------------------------------------------" << std::endl ;
        std::cout << "********************** Llamada al metodo ServerTest **********************" << std::endl ;
        if (c.execute("ServerTest", noArgs, result))
          std::cout << result << "\n\n";
        else
          std::cout << "Error en la llamada a 'ServerTest'\n\n";
        break;
      case 3:
        std::cout << "---------------------------------------------------------------------" << std::endl ;
        std::cout << "******************* Llamada al metodo Eco *******************" << std::endl ;
        oneArg[0] = "Pinocho";
        if (c.execute("Eco", oneArg, result))
          std::cout << result << "\n\n";
        else
          std::cout << "Error en la llamada a 'Eco'\n\n";
        break;
      case 4:
        std::cout << "---------------------------------------------------------------------" << std::endl ;
        std::cout << "****************** Llamada con un array de numeros *******************" << std::endl ;
        numbers[0] = 33.33;
        numbers[1] = 112.57;
        numbers[2] = 76.1;
        std::cout << "numbers.size() is " << numbers.size() << std::endl;
        if (c.execute("Sumar", numbers, result))
          std::cout << "Suma = " << double(result) << "\n\n";
        else
          std::cout << "Error en la llamada a 'Sumar'\n\n";
        break;
      case 5:
        std::cout << "---------------------------------------------------------------------" << std::endl ;
        std::cout << "*********** Prueba de fallo por llamada a metodo inexistente *********" << std::endl ;        
        if (c.execute("MetodoX", numbers, result)){
          std::cout << "Llamada a MetodoX: fallo: " << c.isFault() << std::endl; 
          std::cout << "  con resultado = " << result << std::endl;
        }
        else
          std::cout << "Error en la llamada a 'Sumar'\n";
        break;
      case 6:
        std::cout << "---------------------------------------------------------------------" << std::endl ;
        std::cout << "**************** Prueba de llamada a metodos multiples ***************" << std::endl;
        // En este caso se trata de argumento unico, un array de estructuras
        XmlRpcValue multicall;
        multicall[0][0]["methodName"] = "Sumar";
        multicall[0][0]["params"][0] = 5.0;
        multicall[0][0]["params"][1] = 9.0;

        multicall[0][1]["methodName"] = "Eco";
        multicall[0][1]["params"][0] = "Juan";
          
        multicall[0][2]["methodName"] = "Sumar";
        multicall[0][2]["params"][0] = 10.5;
        multicall[0][2]["params"][1] = 12.5;
        multicall[0][2]["params"][2] = -3.0;

        if (c.execute("system.multicall", multicall, result))
          std::cout << "\nResultado multicall = " << result << std::endl;
        else
          std::cout << "\nError en la llamada a 'system.multicall'\n";
        break;
    }
    cont++;
    if(cont==7) break;
  }

  return 0;
}
