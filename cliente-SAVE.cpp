#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string>
#include <iostream>

#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMDocumentType.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMNodeIterator.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMText.hpp>

#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/XMLUni.hpp>

#include <string>
#include <stdexcept>

#define PORT 8080

using namespace xercesc;
using namespace std;

int cria_socket() {
	struct sockaddr_in address;
	int opt = 1;

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Creating socket file descriptor
    if(client_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if(setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;

    #ifdef DEBUG_ON
    printf("%i\n", INADDR_ANY);
    #endif

    address.sin_port = htons(PORT);

    if(connect(client_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    return client_fd;
}

string arquivo_para_sting(string arquivo) {
    // Abre o arquivo
    FILE *requisicao_xml;
    requisicao_xml = fopen(arquivo.c_str(), "r");

    // Guarda o tamanho do arquivo
    fseek(requisicao_xml, 0, SEEK_END);
    size_t size = ftell(requisicao_xml);

    // Variável para armazenar o conteudo do arquivo
    char *requisicao_aux = new char[size];

    // Volta para o início do arquivo
    rewind(requisicao_xml);

    // Salva o conteúdo do arquivo na variavel requisicao_aux
    fread(requisicao_aux, sizeof(char*), size, requisicao_xml);

    // Salva o conteudo da variavel requisicao_aux na variavel requisicao
    return requisicao_aux;
}

int valida_xml(string xml) {
    int resultado = 1;

    XMLPlatformUtils::Initialize();
    {
        XercesDOMParser domParser;

        string xsd_str = arquivo_para_sting("resposta.xsd");

        if (domParser.loadGrammar(xsd_str.c_str(), Grammar::SchemaGrammarType) == NULL){
            resultado = 0;
        }

        //ParserErrorHandler parserErrorHandler;
        //domParser.setErrorHandler(&parserErrorHandler);

        domParser.setValidationScheme(XercesDOMParser::Val_Always);
        domParser.setDoNamespaces(true);
        domParser.setDoSchema(true);
        domParser.setValidationSchemaFullChecking(true);

        domParser.setExternalNoNamespaceSchemaLocation(xsd_str.c_str());

        domParser.parse(xml.c_str());

        if(domParser.getErrorCount() != 0) {
            resultado = 0;
        }
    }
    XMLPlatformUtils::Terminate();

    return resultado;
}

int verifica_resultado(string xml) {
    XMLPlatformUtils::Initialize();
    {
        XercesDOMParser domParser;
        domParser.parse(xml.c_str());

        DOMDocument *doc = domParser.getDocument();
        DOMElement *root = doc->getDocumentElement();
        DOMNode *node;
        //XMLCh* tmpstr;

        XMLCh *tmpstr = XMLString::transcode("resposta");

        int len;
        int i;

        //XMLString::transcode("retorno", tmpstr ,50);

        DOMNodeList *list = doc->getElementsByTagName(tmpstr);
        len = list->getLength();

        for (i = 0; i < len; i++){
            //Returns DOMnode object
            node = list->item(i);

            const XMLCh* n = node->getNodeValue();
            cout << "Index:" << i << " Value:" << n << endl;
        }
    }
    XMLPlatformUtils::Terminate();
    return 0;
}

int envia_requisicao(string requisicao) {
    char buffer[1024] = {0};
    int socket_id = cria_socket();

    // Envia requisição para o servidor
    //send(socket_id , requisicao , strlen(requisicao.c_str()) , 0);
    send(socket_id, requisicao.c_str(), requisicao.size(), 0);
    printf("\nRequisição Enviada\n");

    // Recebe resposta do servidor e salva na variavel resposta
    read(socket_id, buffer, 1024);

    string resposta = buffer;
    //printf("%s\n", resposta);

    // Verifica se o XML recebido é válido de acordo com o XSD
    int xml_valido = valida_xml(resposta);
    if(xml_valido != 1){
        printf ("\nXML não válido\n");
        exit (EXIT_FAILURE);
    }

    // Verifica o resultado da requisição enviada
    int resultado = verifica_resultado(resposta);

    return resultado;
}

int submeter(string boletim){
    string requisicao = arquivo_para_sting("submeter.xml");
    string historico = arquivo_para_sting(boletim);

    // Preenche o campo valor com o histórico, apagando "[String do XML do historico]"
    int posicao = 0;
    string requisicao_aux = "";
    for(char& c : requisicao) {
        if(posicao == 0){
            if(c != '[')
                requisicao_aux += c;
            else {
                requisicao_aux += "<![CDATA[" + historico + "]]>";
                posicao++;
            }
        } else if(posicao == 1){
            if(c == ']')
                posicao++;
        } else {
            requisicao_aux += c;
        }
    }

    requisicao = requisicao_aux;
    //printf("%s\n", requisicao.c_str());

    return envia_requisicao(requisicao);
}

int consultaStatus(string cpf) {
    string requisicao = arquivo_para_sting("consultaStatus.xml");
    //printf("%s\n", requisicao.c_str());

    // Preenche o campo valor com o cpf escolhido, apagando o valor padrão do xml "00000000002"
    int posicao = 0;
    string requisicao_aux = "";
    for(char& c : requisicao) {
        if(posicao == 0){
            if(c != '0')
                requisicao_aux += c;
            else {
                requisicao_aux += cpf;
                posicao++;
            }
        } else if(posicao == 1){
            if(c == '2')
                posicao++;
        } else {
            requisicao_aux += c;
        }
    }

    requisicao = requisicao_aux;
    //printf("%s\n", requisicao.c_str());

    return envia_requisicao(requisicao);
}

int main(void) {
    string requisicao;
    int escolha = 0;

    do {
        printf("\nDIGITE:\
                \n1 - Para consultar status\
                \n2 - Para submeter um histórico\
                \nOutro valor - Sair\
                \n-->");
        scanf("%d", &escolha);

        if(escolha == 1){
            int escolha_cpf;
            printf("\nEscolha um cpf:\
                    \n0 - 00000000000\
                    \n1 - 00000000001\
                    \n2 - 00000000002\
                    \n3 - 00000000003\
                    \n4 - 00000000004\
                    \n-->");
            scanf("%d", &escolha_cpf);

            string cpf = "";
            switch (escolha_cpf) {
                case 0:
                    cpf = "00000000000";
                    break;
                case 1:
                    cpf = "00000000001";
                    break;
                case 2:
                    cpf = "00000000002";
                    break;
                case 3:
                    cpf = "00000000003";
                    break;
                case 4:
                    cpf = "00000000004";
                    break;
            }

            int resultado = consultaStatus(cpf);

            switch (resultado) {
                case 0:
                    printf("Candidato não encontrado!\n");
                    break;
                case 1:
                    printf("Em processamento!\n");
                    break;
                case 2:
                    printf("Candidato Aprovado e Selecionado!\n");
                    break;
                case 3:
                    printf("Candidato Aprovado e em Espera!\n");
                    break;
                case 4:
                    printf("Candidato Não Aprovado!\n");
                    break;
            }

        } else if(escolha == 2){
            int resultado = submeter("historico-ex.xml");

            switch (resultado) {
                case 0:
                    printf("Sucesso!\n");
                    break;
                case 1:
                    printf("XML inválido!\n");
                    break;
                case 2:
                    printf("XML mal-formado!\n");
                    break;
                case 3:
                    printf("Erro Interno!\n");
                    break;
            }
        }

        /* PODE APAGAR SE QUISER
    	strcat(requisicao, "\n");

    	send(socket_id , requisicao , strlen(requisicao) , 0);
    	printf("Message sent\n");

    	valread = read(socket_id, buffer, 1024);
    	printf("%s\n", buffer);

    	char *bye = "Bye\n";

    	send(socket_id , bye , strlen(bye) , 0);
        */

    } while(escolha == 1 || escolha == 2);

	return 0;
}
