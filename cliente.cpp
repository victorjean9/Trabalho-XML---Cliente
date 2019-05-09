#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string>
#include <iostream>

#include <xercesc/dom/DOM.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>

// #include <xercesc/dom/DOMDocument.hpp>
// #include <xercesc/dom/DOMDocumentType.hpp>
// #include <xercesc/dom/DOMElement.hpp>
// #include <xercesc/dom/DOMImplementation.hpp>
// #include <xercesc/dom/DOMImplementationLS.hpp>
// #include <xercesc/dom/DOMNodeIterator.hpp>
// #include <xercesc/dom/DOMNodeList.hpp>
// #include <xercesc/dom/DOMText.hpp>
// #include <xercesc/util/XMLUni.hpp>
// #include <stdexcept>

#define PORT 8080

using namespace xercesc;
using namespace std;

// Classe para exibir os possíveis erros na validação de um XML com o XSD
class ParserErrorHandler : public ErrorHandler{
    private:
        void reportParseException(const SAXParseException& ex) {
            char* msg = XMLString::transcode(ex.getMessage());
            fprintf(stderr, "na linha %llu colunda %llu, %s\n",
                    ex.getColumnNumber(), ex.getLineNumber(), msg);
            XMLString::release(&msg);
        }

    public:
        void warning(const SAXParseException& ex) {
            reportParseException(ex);
        }

        void error(const SAXParseException& ex) {
            reportParseException(ex);
        }

        void fatalError(const SAXParseException& ex) {
            reportParseException(ex);
        }

        void resetErrors(){
        }
};

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

	// Inicializa o XERCES
    XMLPlatformUtils::Initialize();
    {
        XercesDOMParser domParser;

		// Carrega o XSD no xerces
        if (domParser.loadGrammar("resposta.xsd", Grammar::SchemaGrammarType) == NULL){
            resultado = 0;
        }

		// Garante o tratamento de erros
        ParserErrorHandler parserErrorHandler;
        domParser.setErrorHandler(&parserErrorHandler);

        domParser.setValidationScheme(XercesDOMParser::Val_Always);
        domParser.setDoNamespaces(true);
        domParser.setDoSchema(true);
        domParser.setValidationSchemaFullChecking(true);

        domParser.setExternalNoNamespaceSchemaLocation("resposta.xsd");

		// Cria um arquivo XML temporário na memória
		MemBufInputSource src((const XMLByte*)xml.c_str(), xml.length(), "dummy", false);

		// Carrega o arquivo XML temporário no xerces
		domParser.parse(src);

		// Se houver erros, resultado = 0 para o código não prosseguir
        if(domParser.getErrorCount() != 0) {
            resultado = 0;
        }
    }
	// Fecha o xerces
    XMLPlatformUtils::Terminate();

    return resultado;
}

int verifica_resultado(string xml) {
	// Inicializa o XERCES
    XMLPlatformUtils::Initialize();
    {
        XercesDOMParser domParser;

		// Cria um arquivo XML temporário na memória
		MemBufInputSource src((const XMLByte*)xml.c_str(), xml.length(), "dummy", false);

		// Carrega o arquivo XML temporário no xerces
		domParser.parse(src);

        DOMElement* docRootNode;
        DOMDocument* doc;
        DOMNodeIterator * walker;
        doc = domParser.getDocument();
        docRootNode = doc->getDocumentElement();

		// Iterador para o XML
        walker = doc->createNodeIterator(docRootNode,DOMNodeFilter::SHOW_ELEMENT,NULL,true);

        DOMNode * current_node = NULL;
        string thisNodeName;
        string parentNodeName;
        string valor_retornado = "";

		// Percorre o XML
        for (current_node = walker->nextNode(); current_node != 0; current_node = walker->nextNode()) {

            thisNodeName = XMLString::transcode(current_node->getNodeName());
            parentNodeName = XMLString::transcode(current_node->getParentNode()->getNodeName());

			// Entra nas tags desejadas
            if(parentNodeName == "resposta" ) {
                if(thisNodeName == "retorno") {

					// Pega o valor dentro da tag retorno
                    valor_retornado = XMLString::transcode(current_node->getFirstChild()->getNodeValue());

                    int valor_retorno = std::atoi (valor_retornado.c_str());
                    return valor_retorno;
                }
            }
        }
    }
    XMLPlatformUtils::Terminate();
}

int envia_requisicao(string requisicao) {
    char buffer[1024] = {0};
    int socket_id = cria_socket();

    // Envia requisição para o servidor
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

            printf("\n--- RESULTADO ---\n");

            switch (resultado) {
                case 0:
                    printf("\nCandidato não encontrado!\n");
                    break;
                case 1:
                    printf("\nEm processamento!\n");
                    break;
                case 2:
                    printf("\nCandidato Aprovado e Selecionado!\n");
                    break;
                case 3:
                    printf("\nCandidato Aprovado e em Espera!\n");
                    break;
                case 4:
                    printf("\nCandidato Não Aprovado!\n");
                    break;
            }

        } else if(escolha == 2){
            int resultado = submeter("historico-ex.xml");

            printf("\n--- RESULTADO ---\n");

            switch (resultado) {
                case 0:
                    printf("\nSucesso!\n");
                    break;
                case 1:
                    printf("\nXML inválido!\n");
                    break;
                case 2:
                    printf("\nXML mal-formado!\n");
                    break;
                case 3:
                    printf("\nErro Interno!\n");
                    break;
            }
        }

    } while(escolha == 1 || escolha == 2);

	return 0;
}
