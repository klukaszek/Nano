#include "wgsl-parser.h"
#include "webgpu.h"

void init_parser(Parser *parser, const char *input) {
    parser->input = input;
    parser->position = 0;
}

char peek(Parser *parser) { return parser->input[parser->position]; }

char next(Parser *parser) { return parser->input[parser->position++]; }

int is_eof(Parser *parser) { return peek(parser) == '\0'; }

void skip_whitespace(Parser *parser) {
    while (isspace(peek(parser))) {
        next(parser);
    }
}

int parse_number(Parser *parser) {
    int result = 0;
    while (isdigit(peek(parser))) {
        result = result * 10 + (next(parser) - '0');
    }
    return result;
}

void parse_identifier(Parser *parser, char *ident, bool is_type) {
    int i = 0;
    if (!is_type) {
        while (isalnum(peek(parser)) || peek(parser) == '_') {
            ident[i++] = next(parser);
            if (i >= MAX_IDENT_LENGTH - 1)
                break;
        }
    } else {
        while (isalnum(peek(parser)) || peek(parser) == '_' || peek(parser) == '<' ||
               peek(parser) == '>') {
            ident[i++] = next(parser);
            if (i >= MAX_IDENT_LENGTH - 1)
                break;
        }
    }

    ident[i] = '\0';
}

WGPUBufferUsageFlags parse_storage_class_and_access(Parser *parser) {
    WGPUBufferUsageFlags flags = WGPUBufferUsage_None;
    char identifier[MAX_IDENT_LENGTH];

    parse_identifier(parser, identifier, false);

    printf("Identifier: %s\n", identifier);

    if (strcmp(identifier, "uniform") == 0) {
        flags |= WGPUBufferUsage_Uniform;
    } else if (strcmp(identifier, "storage") == 0) {
        flags |= WGPUBufferUsage_Storage;
    }

    skip_whitespace(parser);
    if (peek(parser) == ',') {
        next(parser); // Skip ','
        skip_whitespace(parser);
        parse_identifier(parser, identifier, false);

        if (strcmp(identifier, "read") == 0) {
            flags |= WGPUBufferUsage_CopySrc;
        } else if (strcmp(identifier, "write") == 0) {
            flags |= WGPUBufferUsage_CopyDst;
        } else if (strcmp(identifier, "read_write") == 0) {
            flags |= WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst;
        }
    }

    return flags;
}

// Parse the binding information from the shader source
void parse_binding(Parser *parser, ShaderInfo *info) {
    skip_whitespace(parser);
    if (next(parser) != '@' ||
        strncmp(&parser->input[parser->position], "group", 5) != 0) {
        return;
    }
    parser->position += 5;

    skip_whitespace(parser);
    next(parser); // Skip '('
    int group = parse_number(parser);
    next(parser); // Skip ')'

    skip_whitespace(parser);
    if (strncmp(&parser->input[parser->position], "@binding", 8) != 0) {
        return;
    }
    parser->position += 8;

    skip_whitespace(parser);
    next(parser); // Skip '('
    int binding = parse_number(parser);
    next(parser); // Skip ')'

    skip_whitespace(parser);
    if (strncmp(&parser->input[parser->position], "var", 3) != 0) {
        return;
    }
    parser->position += 3;

    skip_whitespace(parser);
    next(parser); // Skip '<'

    WGPUBufferUsageFlags usage_flags = parse_storage_class_and_access(parser);

    next(parser); // Skip '>'

    skip_whitespace(parser);
    char name[MAX_IDENT_LENGTH];
    parse_identifier(parser, name, false);

    skip_whitespace(parser);
    next(parser); // Skip ':'

    skip_whitespace(parser);
    char type[MAX_IDENT_LENGTH];
    parse_identifier(parser, type, true);

    BindingInfo *bi = &info->bindings[info->binding_count++];
    bi->group = group;
    bi->binding = binding;
    bi->shader_id = info->id;
    bi->usage_flags = usage_flags;
    strcpy(bi->type, type);
    strcpy(bi->name, name);
}

void parse_entry_point(Parser *parser, ShaderInfo *info) {
    skip_whitespace(parser);
    if (next(parser) != '@') {
        return;
    }

    char attr[MAX_IDENT_LENGTH];
    parse_identifier(parser, attr, false);

    int index = info->entry_point_count;
    EntryPoint *ep = &info->entry_points[info->entry_point_count++];
    if (strcmp(attr, "compute") == 0) {
        ep->type = COMPUTE;
    } else if (strcmp(attr, "vertex") == 0) {
        ep->type = VERTEX;
    } else if (strcmp(attr, "fragment") == 0) {
        ep->type = FRAGMENT;
    } else {
        info->entry_point_count--;
        return;
    }

    skip_whitespace(parser);
    if (strncmp(&parser->input[parser->position], "@workgroup_size", 15) == 0) {
        parser->position += 15;
        skip_whitespace(parser);
        next(parser); // Skip '('
        ep->workgroup_size.x = parse_number(parser);
        if (next(parser) == ',') {
            ep->workgroup_size.y = parse_number(parser);
            if (next(parser) == ',') {
                ep->workgroup_size.z = parse_number(parser);
            }
        }
        next(parser); // Skip ')'
        skip_whitespace(parser);
    }

    if (strncmp(&parser->input[parser->position], "fn", 2) == 0) {
        parser->position += 2;
        skip_whitespace(parser);
        parse_identifier(parser, ep->entry, false);
    }
}

void parse_shader(Parser *parser, ShaderInfo *info) {
    while (!is_eof(parser)) {
        skip_whitespace(parser);
        if (peek(parser) == '@') {
            int saved_position = parser->position;
            next(parser); // Skip '@'
            char attr[MAX_IDENT_LENGTH];
            parse_identifier(parser, attr, false);
            parser->position = saved_position;

            if (strcmp(attr, "group") == 0) {
                parse_binding(parser, info);
            } else if (strcmp(attr, "compute") == 0 ||
                       strcmp(attr, "vertex") == 0 ||
                       strcmp(attr, "fragment") == 0) {
                parse_entry_point(parser, info);
            } else {
                next(parser); // Skip unrecognized attribute
            }
        } else {
            next(parser); // Skip other tokens
        }
    }
}

void print_shader_info(ShaderInfo *info) {
    printf("Bindings:\n");
    for (int i = 0; i < info->binding_count; i++) {
        BindingInfo *bi = &info->bindings[i];
        printf("  @group(%d) @binding(%d) var<", bi->group, bi->binding);

        if (bi->usage_flags & WGPUBufferUsage_Uniform) {
            printf("uniform");
        } else if (bi->usage_flags & WGPUBufferUsage_Storage) {
            printf("storage");
        }

        if (bi->usage_flags & WGPUBufferUsage_CopySrc) {
            if (bi->usage_flags & WGPUBufferUsage_CopyDst) {
                printf(", read_write");
            } else {
                printf(", read");
            }
        } else if (bi->usage_flags & WGPUBufferUsage_CopyDst) {
            printf(", write");
        }

        printf("> %s: %s\n", bi->name, bi->type);

        printf("    Buffer Usage Flags: ");
        if (bi->usage_flags & WGPUBufferUsage_MapRead)
            printf("MapRead ");
        if (bi->usage_flags & WGPUBufferUsage_MapWrite)
            printf("MapWrite ");
        if (bi->usage_flags & WGPUBufferUsage_CopySrc)
            printf("CopySrc ");
        if (bi->usage_flags & WGPUBufferUsage_CopyDst)
            printf("CopyDst ");
        if (bi->usage_flags & WGPUBufferUsage_Index)
            printf("Index ");
        if (bi->usage_flags & WGPUBufferUsage_Vertex)
            printf("Vertex ");
        if (bi->usage_flags & WGPUBufferUsage_Uniform)
            printf("Uniform ");
        if (bi->usage_flags & WGPUBufferUsage_Storage)
            printf("Storage ");
        if (bi->usage_flags & WGPUBufferUsage_Indirect)
            printf("Indirect ");
        if (bi->usage_flags & WGPUBufferUsage_QueryResolve)
            printf("QueryResolve ");
        printf("\n");
    }

    printf("\nEntry Points:\n");
    for (int i = 0; i < info->entry_point_count; i++) {
        EntryPoint *ep = &info->entry_points[i];
        printf("  ");
        switch (ep->type) {
            case COMPUTE:
                printf("@compute");
                if (ep->workgroup_size.x > 0) {
                    printf(" @workgroup_size(%d", ep->workgroup_size.x);
                    if (ep->workgroup_size.y > 0) {
                        printf(", %d", ep->workgroup_size.y);
                        if (ep->workgroup_size.z > 0) {
                            printf(", %d", ep->workgroup_size.z);
                        }
                    }
                    printf(")");
                }
                break;
            case VERTEX:
                printf("@vertex");
                break;
            case FRAGMENT:
                printf("@fragment");
                break;
        }
        printf(" fn %s()\n", ep->entry);
    }
}

// File IO
// -----------------------------------------------
// Function to read a shader into a string
char *read_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Could not open file %s\n", filename);
        return NULL;
    }

    // Get the length of the file so we can allocate the correct amount of
    // memory for the shader string buffer
    fseek(file, 0, SEEK_END);
    size_t length = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    // It is important to free the buffer after using it, it might be a good
    // idea to implement an arena structure into the Nano STL for better
    // handling of dynamic memory allocations.
    char *buffer = (char *)malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    return buffer;
}
