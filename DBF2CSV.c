#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

//WARNING: Esse código parece não ler a primeira linha do arquivo dbf. consertar mais tarde

// Constantes globais
const uint8_t OEM = 25;               // Valor OEM (Original Equipment Manufacturer)
const uint16_t MAX_NUM_FIELDS = 90;   // Número máximo de campos permitidos

// Estrutura que representa um campo no arquivo DBF
typedef struct _FIELD {
    uint8_t fieldSize;  // Tamanho do campo
    uint8_t fieldType;  // Tipo do campo (ex: 'C' para caractere, 'N' para numérico)
    char* fieldName;    // Nome do campo
} DBF_FIELD;

// Estrutura que armazena informações gerais sobre o arquivo DBF
typedef struct DBF_INFO {
    uint32_t nLines;        // Número de linhas no arquivo
    uint32_t nCols;         // Número de colunas (campos) no arquivo
    uint16_t lenHeader;     // Tamanho do cabeçalho do arquivo
    uint16_t sizeOfRecord;  // Tamanho de cada registro (linha) no arquivo
    DBF_FIELD *fields;      // Array de campos (colunas) do arquivo
} DBF_INFO;

// Protótipos das funções
DBF_INFO *new_dbf_info();
DBF_INFO *get_dbf_info(FILE *file);
void verify_file_existence(FILE *file);
uint32_t get_nlines(FILE *file);
uint16_t get_len_header(FILE *file);
uint16_t get_len_record(FILE *file);
uint16_t get_fields(FILE *file, DBF_FIELD *fields);
void process_dbf_body(FILE *file, DBF_INFO *dbf_info, FILE* csv_file);
void process_entry_n(FILE *dbf_file, FILE *csv_file, DBF_INFO *dbf_info, uint32_t n);
void process_header(FILE *csv_file, DBF_INFO *dbf_info);
void str_subs(char* str, char changed, char changer, int til);

// Função principal
// Uso esperado: ./dbf2c <arquivo_fonte> <arquivo_destino>
int main(int n_args, char **argv) {
    char *arquivo_fonte = argv[1];    // Nome do arquivo DBF de origem
    char *arquivo_destino = argv[2];  // Nome do arquivo CSV de destino

    // Abre o arquivo DBF
    FILE *dbf_file = fopen(arquivo_fonte, "r");
    verify_file_existence(dbf_file);  // Verifica se o arquivo existe

    // Obtém as informações do arquivo DBF
    DBF_INFO *dbf_file_info = get_dbf_info(dbf_file);

    // Exibe algumas informações do arquivo DBF
    printf("linhas: %d\ntamanaho header %d\ntamanho de uma linha %d\nnumero de colunas %d\n",
           dbf_file_info->nLines,
           dbf_file_info->lenHeader,
           dbf_file_info->sizeOfRecord,
           dbf_file_info->nCols);

    // Processa a header do arquivo DBF e converte para CSV

    // Processa o corpo do arquivo DBF e converte para CSV
    FILE* csv_file = fopen(arquivo_destino, "w");
    process_header(csv_file, dbf_file_info);
    process_dbf_body(dbf_file, dbf_file_info, csv_file);
}

// Função para criar uma nova estrutura DBF_INFO
DBF_INFO *new_dbf_info() {
    DBF_INFO *info = malloc(sizeof(DBF_INFO));
    return info;
}

// Função para obter informações do arquivo DBF
DBF_INFO *get_dbf_info(FILE *file) {
    uint32_t nLines = get_nlines(file);               // Obtém o número de linhas
    uint16_t n_bytes_no_header = get_len_header(file); // Obtém o tamanho do cabeçalho
    uint16_t n_bytes_per_record = get_len_record(file); // Obtém o tamanho de cada registro
    DBF_FIELD *fields = malloc(sizeof(DBF_FIELD) * MAX_NUM_FIELDS); // Aloca memória para os campos
    uint16_t nFields = get_fields(file, fields);      // Obtém os campos do arquivo

    // Preenche a estrutura DBF_INFO com as informações obtidas
    DBF_INFO *dbf_info = new_dbf_info();
    dbf_info->nCols = nFields;
    dbf_info->nLines = nLines;
    dbf_info->lenHeader = n_bytes_no_header;
    dbf_info->sizeOfRecord = n_bytes_per_record;
    dbf_info->fields = fields;

    return dbf_info;
}

// Função para processar o corpo do arquivo DBF e convertê-lo para CSV
void process_dbf_body(FILE *dbf_file, DBF_INFO *dbf_info, FILE* csv_file) {
    uint32_t n = 0;

    // Processa cada entrada (linha) do arquivo DBF
    while (fgetc(dbf_file) != 0x1a) {  // 0x1a é o marcador de fim de arquivo em DBF
        process_entry_n(dbf_file, csv_file, dbf_info, n);
        n++;
    }
}

void process_header(FILE *csv_file, DBF_INFO *dbf_info) {
    for (int i = 0; i < dbf_info->nCols; i++) {
        if (i < dbf_info->nCols-1) {
            fprintf(csv_file, "%s,", dbf_info->fields[i].fieldName);
        } else {
            fprintf(csv_file, "%s\n", dbf_info->fields[i].fieldName);
        }
    }
}

// Função para processar uma entrada específica (linha) do arquivo DBF
void process_entry_n(FILE *dbf_file, FILE *csv_file, DBF_INFO *dbf_info, uint32_t n) {
    uint16_t start_of_data = dbf_info->lenHeader;  // Posição inicial dos dados
    uint16_t size_of_entry = dbf_info->sizeOfRecord; // Tamanho de cada registro
    void* cell_buffer = malloc(256);  // Buffer para armazenar o conteúdo de uma célula

    // Posiciona o ponteiro do arquivo no início do registro desejado
    fseek(dbf_file, start_of_data + (n * size_of_entry), SEEK_SET);

    char line_flag = fgetc(dbf_file);  // Lê o flag de linha (indica se a linha está ativa ou deletada)

    if (line_flag == 0x2a){ // se o arquivo foi deletado ignore-o
        return;
    }
    
    // Processa cada campo (coluna) do registro
    for (int i = 0; i < dbf_info->nCols; i++) {
        uint8_t size_of_cell = dbf_info->fields[i].fieldSize;  // Tamanho do campo
        uint8_t type_of_data = dbf_info->fields[i].fieldType;  // Tipo do campo

        // Lê o conteúdo do campo
        fread(cell_buffer, size_of_cell, 1, dbf_file);

        str_subs(cell_buffer, ',', '.', 14);
        // Processa o campo 
        if (i < dbf_info->nCols-1) {
            ((char*)cell_buffer)[size_of_cell] = '\0';  // Adiciona terminador nulo
            fprintf(csv_file, "%s,", (char*)cell_buffer);
        } else {
            ((char*)cell_buffer)[size_of_cell] = '\0';  // Adiciona terminador nulo
            fprintf(csv_file, "%s\n", (char*)cell_buffer);
        }
    }
}

void str_subs(char* str, char changed, char changer, int til) {
    for (int i = 0; i < til; i++){
        if (str[i] == changed){
            str[i] = changer;
        }
    }
}

// Função para obter o número de linhas no arquivo DBF
uint32_t get_nlines(FILE *file) {
    uint32_t nlines;
    fseek(file, 4, SEEK_SET);  // Posiciona o ponteiro no local onde o número de linhas está armazenado
    fread(&nlines, sizeof(uint32_t), 1, file);  // Lê o número de linhas
    return nlines;
}

// Função para obter o tamanho do cabeçalho do arquivo DBF
uint16_t get_len_header(FILE *file) {
    uint16_t n_bytes_header;
    fseek(file, 8, SEEK_SET);  // Posiciona o ponteiro no local onde o tamanho do cabeçalho está armazenado
    fread(&n_bytes_header, sizeof(uint16_t), 1, file);  // Lê o tamanho do cabeçalho
    return n_bytes_header;
}

// Função para obter o tamanho de cada registro (linha) no arquivo DBF
uint16_t get_len_record(FILE *file) {
    uint16_t n_bytes_header;
    fseek(file, 10, SEEK_SET);  // Posiciona o ponteiro no local onde o tamanho do registro está armazenado
    fread(&n_bytes_header, sizeof(uint16_t), 1, file);  // Lê o tamanho do registro
    return n_bytes_header;
}

// Função para obter os campos (colunas) do arquivo DBF
uint16_t get_fields(FILE *file, DBF_FIELD *fields) {
    uint8_t field_size;
    uint8_t field_type;
    uint16_t nFields = 0;
    char charFlag;

    fseek(file, 32, SEEK_SET);  // Posiciona o ponteiro no início da área de campos

    // Lê cada campo até encontrar o marcador de fim de campos (0x0d)
    while ((charFlag = fgetc(file)) != 0x0d) {
        fseek(file, -1, SEEK_CUR);  // Volta uma posição para ler o campo corretamente

        char* field_name = malloc(11);  // Aloca memória para o nome do campo
        fread(field_name, 11, 1, file);  // Lê o nome do campo
        fread(&field_type, sizeof(field_type), 1, file);  // Lê o tipo do campo
        fseek(file, 4, SEEK_CUR);  // Pula 4 bytes (reservados)
        fread(&field_size, sizeof(field_size), 1, file);  // Lê o tamanho do campo

        // Armazena o campo na estrutura DBF_FIELD
        fields[nFields] = (DBF_FIELD){field_size, field_type, field_name};
        nFields++;

        // Posiciona o ponteiro no próximo campo
        fseek(file, 32 + (32 * nFields), SEEK_SET);
    }

    return nFields;  // Retorna o número de campos encontrados
}

// Função para verificar se o arquivo existe
void verify_file_existence(FILE *file) {
    if (file == NULL) {
        printf("o arquivo dbf indicado não existe");
        exit(1);
    }
}
