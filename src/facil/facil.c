/* FACILITIES.DLL -- module id 37: the compiled facility index.
 *
 * FE exposes the facility-directory blocks embedded in scenery files.  FACIL
 * turns those blocks into INDEX_FILE, a small hierarchy used by the airport
 * and scenery dialogs.  This file currently covers the non-UI half: profile
 * state, index validation, hierarchy construction and serialization.  The
 * three dialog services remain deliberately inert until their ofstest pass.
 *
 * The complete retail .data image is retained.  Besides making the recovered
 * offsets useful documentation, this preserves the dialog tables while the UI
 * half is still being transliterated.  Like retail, this is a 32-bit Win32 ABI
 * module and the image contains live pointers.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define FACIL_IMAGE_SIZE 0x2d1c
#define FACIL_DATA_START 0x0074
#define FACIL_NSLOTS     7

typedef uint32_t __attribute__((may_alias)) facil_u32;

typedef void (*facil_fn)(void);

struct facil_linkage {
    uint32_t id;
    uint32_t (FS98_STDCALL *init)(void);
    uint32_t (FS98_STDCALL *deinit)(void);
    uint32_t reserved_0c;
    uint32_t reserved_10;
    void *dialogs;
    facil_fn slots[FACIL_NSLOTS];
};

struct facil_image {
    struct fs98_import import[8];
    struct facil_linkage linkage;
    uint8_t data[FACIL_IMAGE_SIZE - FACIL_DATA_START];
};

extern struct facil_image facil_image;

typedef char (*facil_layout_check)[
    sizeof(struct facil_image) == FACIL_IMAGE_SIZE &&
    sizeof(struct facil_linkage) == 0x34 ? 1 : -1];

#define D                    ((uint8_t *)&facil_image)
#define D32(offset)          (*(facil_u32 *)(D + (offset)))
#define DPTR(offset, type)   (*(type **)(D + (offset)))
#define IMPORT(index)        (facil_image.import[(index)].link)
#define HOST                 IMPORT(0)
#define FE                   IMPORT(3)
#define UTIL                 IMPORT(6)
#define SLOT(module, offset, type) \
    (*(type *)((uint8_t *)(module) + (offset)))

struct facil_source {
    uint8_t id[16];
    uint32_t value;
};

struct facil_tree {
    struct fs98_host_list order;
    struct fs98_host_list nodes;
};

struct facil_node {
    const char *name;
    uint32_t kind;
    struct fs98_host_list sources;
    uint32_t children_ready;
    struct facil_tree children;
};

struct facil_leaf {
    const char *name;
    uint32_t kind;
    struct fs98_host_list sources;
};

struct facil_disk_node {
    uint32_t parent;
    uint32_t sibling;
    uint32_t child;
    uint32_t kind;
    uint32_t payload_size;
    uint32_t flags;
    uint32_t name_size;
    uint32_t source_count;
    char name_and_sources[0x1000];
};

typedef char (*facil_tree_check)[
    sizeof(struct facil_source) == 0x14 &&
    sizeof(struct facil_tree) == 0x38 &&
    sizeof(struct facil_node) == 0x60 &&
    sizeof(struct facil_leaf) == 0x24 ? 1 : -1];

struct fs98_host_file;

typedef int32_t (FS98_STDCALL *facil_cfg_int_fn)(const char *, const char *,
                                                 int32_t *);
typedef int32_t (FS98_STDCALL *facil_cfg_put_int_fn)(const char *, const char *,
                                                     int32_t);
typedef int32_t (FS98_STDCALL *facil_cfg_put_string_fn)(const char *,
                                                        const char *,
                                                        const char *);
typedef int32_t (FS98_STDCALL *facil_get_attributes_fn)(int32_t *, const char *,
                                                        int32_t);
typedef int32_t (FS98_STDCALL *facil_set_attributes_fn)(int32_t, const char *,
                                                        int32_t);
typedef int32_t (FS98_STDCALL *facil_file_create_fn)(struct fs98_host_file **,
                                                     const char *, int32_t);
typedef int32_t (FS98_STDCALL *facil_file_delete_fn)(const char *, int32_t);
typedef int32_t (FS98_STDCALL *facil_file_move_fn)(const char *, int32_t,
                                                   const char *, int32_t);
typedef int32_t (FS98_STDCALL *facil_temp_fn)(char *, int32_t);
typedef int32_t (FS98_STDCALL *facil_file_tell_fn)(struct fs98_host_file *,
                                                   uint32_t *);
typedef int32_t (FS98_STDCALL *facil_file_size_fn)(struct fs98_host_file *,
                                                   uint32_t *);
typedef int32_t (FS98_STDCALL *facil_file_write_fn)(struct fs98_host_file *,
                                                    const void *, uint32_t,
                                                    uint32_t *);
typedef int32_t (FS98_STDCALL *facil_loadstring_alloc_fn)(uint32_t, char **);

typedef uint32_t (FS98_STDCALL *facil_fe_list_fn)(uint8_t **, uint32_t *);
typedef uint32_t (FS98_STDCALL *facil_fe_open_fn)(const uint8_t *, uint8_t **);
typedef uint32_t (FS98_STDCALL *facil_fe_read_fn)(const struct facil_source *,
                                                  uint32_t, uint32_t,
                                                  uint8_t **, uint32_t *);

typedef int32_t (FS98_STDCALL *facil_alert_fn)(void **, uint32_t, uint32_t,
                                               uint32_t, uint32_t, uint32_t,
                                               uint32_t, uint32_t);
typedef int32_t (FS98_STDCALL *facil_alert_close_fn)(void **);

#define HOST_CFG_INT          SLOT(HOST, 0x0a8, facil_cfg_int_fn)
#define HOST_CFG_PUT_INT      SLOT(HOST, 0x0ac, facil_cfg_put_int_fn)
#define HOST_CFG_PUT_STRING   SLOT(HOST, 0x0cc, facil_cfg_put_string_fn)
#define HOST_GET_ATTRIBUTES   SLOT(HOST, 0x134, facil_get_attributes_fn)
#define HOST_SET_ATTRIBUTES   SLOT(HOST, 0x138, facil_set_attributes_fn)
#define HOST_FILE_CREATE      SLOT(HOST, 0x148, facil_file_create_fn)
#define HOST_FILE_DELETE      SLOT(HOST, 0x14c, facil_file_delete_fn)
#define HOST_FILE_MOVE        SLOT(HOST, 0x16c, facil_file_move_fn)
#define HOST_TEMP             SLOT(HOST, 0x174, facil_temp_fn)
#define HOST_FILE_TELL        SLOT(HOST, 0x190, facil_file_tell_fn)
#define HOST_FILE_SIZE        SLOT(HOST, 0x1a4, facil_file_size_fn)
#define HOST_FILE_WRITE       SLOT(HOST, 0x1b0, facil_file_write_fn)
#define HOST_LOADSTRING_ALLOC SLOT(HOST, 0x2a4, facil_loadstring_alloc_fn)

#define FE_LIST               SLOT(FE, 0x7c, facil_fe_list_fn)
#define FE_OPEN               SLOT(FE, 0x80, facil_fe_open_fn)
#define FE_READ               SLOT(FE, 0x84, facil_fe_read_fn)
#define UTIL_ALERT            SLOT(UTIL, 0xa8, facil_alert_fn)
#define UTIL_ALERT_CLOSE      SLOT(UTIL, 0xac, facil_alert_close_fn)

#define ROOT_TREE             ((struct facil_tree *)(D + 0x1af8))
#define LAST_OFFSETS          ((uint32_t *)(D + 0x1ae0))
#define RECORD_BUFFER         ((struct facil_disk_node *)(D + 0x1b30))
#define ACTIVE_ID             DPTR(0x2b30, uint8_t)
#define ACTIVE_REGION         DPTR(0x2b34, struct facil_node)
#define ACTIVE_COUNTRY        DPTR(0x2b38, struct facil_node)
#define ACTIVE_STATE          DPTR(0x2b3c, struct facil_node)
#define ACTIVE_CITY           DPTR(0x2b40, struct facil_node)
#define ACTIVE_AIRPORT        DPTR(0x2b44, struct facil_leaf)
#define EMPTY_NAME            ((const char *)(D + 0x2b48))

static void facil_tree_init(struct facil_tree *tree, uint32_t node_size)
{
    tree->order.count = 0;
    tree->order.capacity = 0;
    tree->order.grow = 0x20;
    tree->order.elem_size = 4;
    tree->order.iterating = 0;
    tree->order.pending = 0;
    tree->order.data = NULL;
    tree->nodes.count = 0;
    tree->nodes.capacity = 0;
    tree->nodes.grow = 0x20;
    tree->nodes.elem_size = (int32_t)node_size;
    tree->nodes.iterating = 0;
    tree->nodes.pending = 0;
    tree->nodes.data = NULL;
}

static void facil_clear_active(void)
{
    ACTIVE_REGION = NULL;
    ACTIVE_COUNTRY = NULL;
    ACTIVE_STATE = NULL;
    ACTIVE_CITY = NULL;
    ACTIVE_AIRPORT = NULL;
}

static int32_t facil_tree_free(struct facil_tree *tree, uint32_t depth)
{
    uint32_t index;

    for (index = (uint32_t)tree->order.count; index != 0; index--) {
        uint32_t offset = ((uint32_t *)tree->order.data)[index - 1];
        struct facil_node *node =
            (struct facil_node *)((uint8_t *)tree->nodes.data + offset);
        void *source_data;

        if (depth != 0 &&
            facil_tree_free(&node->children, depth - 1) != 0)
            return -1;
        source_data = node->sources.data;
        node->sources.count = 0;
        node->sources.capacity = 0;
        node->sources.grow = 0x20;
        node->sources.elem_size = 0x14;
        node->sources.iterating = 0;
        node->sources.pending = 0;
        node->sources.data = NULL;
        if (FS98_HOST_FREE(HOST)(&source_data) != 0)
            return -1;
    }
    {
        void *nodes = tree->nodes.data;
        void *order = tree->order.data;

        facil_tree_init(tree, depth == 0 ? 0x24 : 0x60);
        if (FS98_HOST_FREE(HOST)(&nodes) != 0)
            return -1;
        if (FS98_HOST_FREE(HOST)(&order) != 0)
            return -1;
    }
    return 0;
}

static int32_t facil_reset_tree(void)
{
    if (facil_tree_free(ROOT_TREE, 4) != 0)
        return -1;
    facil_clear_active();
    return 0;
}

static void facil_source_copy(struct facil_source *destination,
                              const uint8_t *id, uint32_t value)
{
    uint32_t index;

    for (index = 0; index < 16; index++)
        destination->id[index] = id[index];
    destination->value = value;
}

static int facil_source_exists(struct fs98_host_list *sources,
                               const uint8_t *id, uint32_t value)
{
    uint32_t index;

    for (index = (uint32_t)sources->count; index != 0; index--) {
        struct facil_source *source =
            (struct facil_source *)sources->data + index - 1;
        if (source->value == value &&
            FS98_HOST_MEMCMP(HOST)(id, source->id, 16) == 0)
            return 1;
    }
    return 0;
}

static void *facil_tree_add(struct facil_tree *tree, uint32_t node_size,
                            const char *name, uint32_t kind,
                            const uint8_t *id, uint32_t value)
{
    int32_t low = 0;
    int32_t high = tree->order.count - 1;
    int32_t insertion = 0;
    struct facil_leaf *node;

    while (low <= high) {
        int32_t middle = (low + high) / 2;
        uint32_t offset = ((uint32_t *)tree->order.data)[middle];
        struct facil_leaf *candidate =
            (struct facil_leaf *)((uint8_t *)tree->nodes.data + offset);
        int32_t comparison = FS98_HOST_STRCMP(HOST)(name, candidate->name);

        if (comparison == 0) {
            if (kind < candidate->kind)
                comparison = -1;
            else if (kind > candidate->kind)
                comparison = 1;
        }
        if (comparison == 0) {
            if (!facil_source_exists(&candidate->sources, id, value)) {
                struct facil_source *source =
                    (struct facil_source *)FS98_HOST_LIST_ADD(HOST)(
                        &candidate->sources);
                facil_source_copy(source, id, value);
            }
            return candidate;
        }
        if (comparison < 0)
            high = middle - 1;
        else
            low = middle + 1;
    }
    insertion = low;
    node = (struct facil_leaf *)FS98_HOST_LIST_ADD(HOST)(&tree->nodes);
    node->name = name;
    node->kind = kind;
    node->sources.count = 0;
    node->sources.capacity = 0;
    node->sources.grow = 0x20;
    node->sources.elem_size = 0x14;
    node->sources.iterating = 0;
    node->sources.pending = 0;
    node->sources.data = NULL;
    {
        struct facil_source *source =
            (struct facil_source *)FS98_HOST_LIST_ADD(HOST)(&node->sources);
        uint32_t *order;
        int32_t index;

        facil_source_copy(source, id, value);
        order = (uint32_t *)FS98_HOST_LIST_ADD(HOST)(&tree->order);
        (void)order;
        order = (uint32_t *)tree->order.data;
        for (index = tree->order.count - 1; index > insertion; index--)
            order[index] = order[index - 1];
        order[insertion] =
            (uint32_t)(tree->nodes.count - 1) * node_size;
    }
    return node;
}

static void facil_prepare_children(struct facil_node *node, uint32_t size)
{
    if (!node->children_ready) {
        facil_tree_init(&node->children, size);
        node->children_ready = 1;
    }
}

static struct facil_node *facil_add_region(const char *name, uint32_t kind,
                                            uint32_t value)
{
    struct facil_node *node;

    if (ACTIVE_REGION == NULL)
        facil_tree_init(ROOT_TREE, 0x60);
    node = (struct facil_node *)facil_tree_add(ROOT_TREE, 0x60, name, kind,
                                               ACTIVE_ID, value);
    ACTIVE_REGION = node;
    ACTIVE_COUNTRY = NULL;
    ACTIVE_STATE = NULL;
    ACTIVE_CITY = NULL;
    ACTIVE_AIRPORT = NULL;
    return node;
}

static struct facil_node *facil_add_country(const char *name, uint32_t kind,
                                             uint32_t value)
{
    if (ACTIVE_REGION == NULL)
        facil_add_region(EMPTY_NAME, 0x10000, 0);
    facil_prepare_children(ACTIVE_REGION, 0x60);
    ACTIVE_COUNTRY = (struct facil_node *)facil_tree_add(
        &ACTIVE_REGION->children, 0x60, name, kind, ACTIVE_ID, value);
    ACTIVE_STATE = NULL;
    ACTIVE_CITY = NULL;
    ACTIVE_AIRPORT = NULL;
    return ACTIVE_COUNTRY;
}

static struct facil_node *facil_add_state(const char *name, uint32_t kind,
                                           uint32_t value)
{
    if (ACTIVE_COUNTRY == NULL)
        facil_add_country(EMPTY_NAME, 0x20000, 0);
    facil_prepare_children(ACTIVE_COUNTRY, 0x60);
    ACTIVE_STATE = (struct facil_node *)facil_tree_add(
        &ACTIVE_COUNTRY->children, 0x60, name, kind, ACTIVE_ID, value);
    ACTIVE_CITY = NULL;
    ACTIVE_AIRPORT = NULL;
    return ACTIVE_STATE;
}

static struct facil_node *facil_add_city(const char *name, uint32_t kind,
                                          uint32_t value)
{
    if (ACTIVE_STATE == NULL)
        facil_add_state(EMPTY_NAME, 0x30000, 0);
    facil_prepare_children(ACTIVE_STATE, 0x60);
    ACTIVE_CITY = (struct facil_node *)facil_tree_add(
        &ACTIVE_STATE->children, 0x60, name, kind, ACTIVE_ID, value);
    ACTIVE_AIRPORT = NULL;
    return ACTIVE_CITY;
}

static struct facil_leaf *facil_add_airport(const uint8_t *record)
{
    if (ACTIVE_CITY == NULL)
        facil_add_city(EMPTY_NAME, 0x40000, 0);
    facil_prepare_children(ACTIVE_CITY, 0x24);
    ACTIVE_AIRPORT = (struct facil_leaf *)facil_tree_add(
        &ACTIVE_CITY->children, 0x24, (const char *)record + 0x10,
        *(const facil_u32 *)(record + 8), ACTIVE_ID,
        *(const facil_u32 *)(record + 0x0c));
    return ACTIVE_AIRPORT;
}

static int32_t facil_parse_block(const uint8_t *block)
{
    uint32_t count = *(const facil_u32 *)(block + 0x14);
    const uint8_t *record = block + 0x18;

    while (count-- != 0) {
        uint32_t kind = *(const facil_u32 *)(record + 8);
        uint32_t value = *(const facil_u32 *)(record + 0x0c);
        const char *name = (const char *)record + 0x10;

        switch (kind) {
        case 0x10001: facil_add_region(name, kind, value); break;
        case 0x20001: facil_add_country(name, kind, value); break;
        case 0x30001: facil_add_state(name, kind, value); break;
        case 0x40001: facil_add_city(name, kind, value); break;
        case 0x50001:
        case 0x50002:
        case 0x50003:
        case 0x50004:
            facil_add_airport(record);
            break;
        }
        record += *(const facil_u32 *)(record + 4) + 8;
    }
    return 0;
}

static int32_t FS98_STDCALL facil_compare_ids(const void *left,
                                               const void *right)
{
    return FS98_HOST_MEMCMP(HOST)(left, right, 16);
}

static void facil_sort_ids(uint8_t *ids, uint32_t count)
{
    FS98_HOST_QSORT(HOST)(ids, count, 16, facil_compare_ids);
}

static void facil_unique_ids(uint8_t *ids, uint32_t *count)
{
    uint32_t read;
    uint32_t write = 1;

    if (*count <= 1)
        return;
    for (read = 1; read < *count; read++) {
        if (FS98_HOST_MEMCMP(HOST)(ids + (write - 1) * 16,
                                   ids + read * 16, 16) != 0) {
            if (write != read)
                FS98_HOST_MEMMOVE(HOST)(ids + write * 16,
                                        ids + read * 16, 16);
            write++;
        }
    }
    *count = write;
}

static int32_t facil_read_old_ids(uint8_t **ids, uint32_t *count)
{
    char name[0x104];
    struct fs98_host_file *file;
    uint32_t got;
    uint32_t bytes;

    *ids = NULL;
    *count = 0;
    FS98_HOST_LOADSTRING(HOST)(0x9101, name, sizeof(name));
    if (FS98_HOST_EXISTS(HOST)(name, 0) != 0)
        return 0;
    if (FS98_HOST_FILE_OPEN(HOST)(&file, name, 0, 0) != 0)
        return -1;
    if (FS98_HOST_FILE_SEEK(HOST)(file, 8, 0) != 0 ||
        FS98_HOST_FILE_READ(HOST)(file, count, 4, &got) != 0 || got != 4)
        return -1;
    bytes = *count * 16;
    if (FS98_HOST_ALLOC(HOST)(1, bytes, (void **)ids, 0) != 0)
        return -1;
    if (FS98_HOST_FILE_READ(HOST)(file, *ids, bytes, &got) != 0)
        return -1;
    if (FS98_HOST_FILE_CLOSE(HOST)(file) != 0)
        return -1;
    return got == bytes ? 0 : -1;
}

static int facil_index_changed(const uint8_t *old_ids, uint32_t old_count,
                               const uint8_t *new_ids, uint32_t new_count)
{
    char *name = NULL;
    uint32_t index;

    HOST_LOADSTRING_ALLOC(0x9101, &name);
    if (FS98_HOST_EXISTS(HOST)(name, 0) != 0) {
        FS98_HOST_FREE(HOST)((void **)&name);
        return 1;
    }
    FS98_HOST_FREE(HOST)((void **)&name);
    if (old_count != new_count)
        return 1;
    for (index = 0; index < old_count; index++)
        if (FS98_HOST_MEMCMP(HOST)(old_ids + index * 16,
                                   new_ids + index * 16, 16) != 0)
            return 1;
    return 0;
}

static int32_t facil_backfill(struct fs98_host_file *file, uint32_t offset,
                              uint32_t value)
{
    uint32_t old_position;
    uint32_t wrote;

    if (HOST_FILE_TELL(file, &old_position) != 0 ||
        FS98_HOST_FILE_SEEK(HOST)(file, offset, 0) != 0 ||
        HOST_FILE_WRITE(file, &value, 4, &wrote) != 0 || wrote != 4 ||
        FS98_HOST_FILE_SEEK(HOST)(file, old_position, 0) != 0)
        return -1;
    return 0;
}

static int32_t facil_or_field(struct fs98_host_file *file, uint32_t offset,
                              uint32_t bits)
{
    uint32_t old_position;
    uint32_t value;
    uint32_t got;
    uint32_t wrote;

    if (HOST_FILE_TELL(file, &old_position) != 0 ||
        FS98_HOST_FILE_SEEK(HOST)(file, offset, 0) != 0 ||
        FS98_HOST_FILE_READ(HOST)(file, &value, 4, &got) != 0 || got != 4)
        return -1;
    value |= bits;
    if (FS98_HOST_FILE_SEEK(HOST)(file, offset, 0) != 0 ||
        HOST_FILE_WRITE(file, &value, 4, &wrote) != 0 || wrote != 4 ||
        FS98_HOST_FILE_SEEK(HOST)(file, old_position, 0) != 0)
        return -1;
    return 0;
}

static int32_t facil_write_node(struct fs98_host_file *file,
                                struct facil_disk_node *record,
                                uint32_t size, uint32_t level,
                                struct facil_node *node);

static int32_t facil_write_tree(struct fs98_host_file *file,
                                struct facil_tree *tree, uint32_t parent,
                                uint32_t level)
{
    uint32_t index;

    for (index = 0; index < (uint32_t)tree->order.count; index++) {
        uint32_t offset = ((uint32_t *)tree->order.data)[index];
        struct facil_node *node =
            (struct facil_node *)((uint8_t *)tree->nodes.data + offset);
        uint32_t name_size;
        uint32_t source_size;
        uint32_t total_size;

        RECORD_BUFFER->parent = parent;
        RECORD_BUFFER->sibling = 0;
        RECORD_BUFFER->child = 0;
        RECORD_BUFFER->kind = node->kind;
        RECORD_BUFFER->flags = 0;
        name_size = FS98_HOST_STRLEN(HOST)(node->name) + 1;
        RECORD_BUFFER->name_size = name_size;
        RECORD_BUFFER->source_count = (uint32_t)node->sources.count;
        FS98_HOST_STRCPY(HOST)(RECORD_BUFFER->name_and_sources, node->name);
        source_size = (uint32_t)node->sources.count * 0x14;
        FS98_HOST_MEMMOVE(HOST)(
            (uint8_t *)RECORD_BUFFER->name_and_sources + name_size,
            node->sources.data, source_size);
        RECORD_BUFFER->payload_size = name_size + source_size + 0x0c;
        total_size = name_size + source_size + 0x20;
        if (facil_write_node(file, RECORD_BUFFER, total_size, level, node) != 0)
            return -1;
        if (node->kind == 0x50001) {
            uint32_t node_offset = LAST_OFFSETS[level];
            uint8_t *section = NULL;
            uint32_t section_size = 0;
            struct facil_disk_node fallback;
            struct facil_source *source =
                (struct facil_source *)node->sources.data;
            const char *fallback_name = EMPTY_NAME;

            if (facil_or_field(file, node_offset + 0x14, 2) != 0)
                return -1;
            if (!FE_READ(source, source->value, 0x10006,
                         &section, &section_size))
                return -1;
            if (section != NULL)
                fallback_name = (const char *)section + 8;
            fallback.parent = parent;
            fallback.sibling = 0;
            fallback.child = 0;
            fallback.kind = 0x50002;
            fallback.flags = 0;
            fallback.name_size = FS98_HOST_STRLEN(HOST)(fallback_name) + 1;
            fallback.source_count = (uint32_t)node->sources.count;
            FS98_HOST_STRCPY(HOST)(fallback.name_and_sources, fallback_name);
            FS98_HOST_MEMMOVE(HOST)(
                (uint8_t *)fallback.name_and_sources + fallback.name_size,
                node->sources.data, source_size);
            fallback.payload_size = fallback.name_size + source_size + 0x0c;
            total_size = fallback.name_size + source_size + 0x20;
            if (facil_write_node(file, &fallback, total_size, level, NULL) != 0)
                return -1;
            if (FS98_HOST_FREE(HOST)((void **)&section) != 0)
                return -1;
        }
    }
    return 0;
}

static int32_t facil_write_node(struct fs98_host_file *file,
                                struct facil_disk_node *record,
                                uint32_t size, uint32_t level,
                                struct facil_node *node)
{
    uint32_t position;
    uint32_t wrote;

    if (HOST_FILE_TELL(file, &position) != 0)
        return -1;
    if (LAST_OFFSETS[level] != 0 &&
        facil_backfill(file, LAST_OFFSETS[level] + 4, position) != 0)
        return -1;
    LAST_OFFSETS[level] = position;
    if (HOST_FILE_WRITE(file, record, size, &wrote) != 0 || wrote != size)
        return -1;
    {
        uint32_t child_position = 0;
        int has_children = level != 0 && node != NULL && node->children_ready;

        if (has_children && HOST_FILE_TELL(file, &child_position) != 0)
            return -1;
        /* Retail backfills this field even when it is already zero.  Besides
         * being visible to the harness, that keeps the file cursor/error
         * behavior identical on a short or otherwise broken write. */
        if (facil_backfill(file, position + 8, child_position) != 0)
            return -1;
        if (has_children &&
            facil_write_tree(file, &node->children, position, level - 1) != 0)
            return -1;
    }
    return 0;
}

static int32_t facil_replace_index(const char *destination,
                                   const char *source)
{
    int32_t attributes;

    if (FS98_HOST_EXISTS(HOST)(destination, 0) == 0) {
        if (HOST_GET_ATTRIBUTES(&attributes, destination, 0) != 0 ||
            HOST_SET_ATTRIBUTES(1, destination, 0) != 0 ||
            HOST_FILE_DELETE(destination, 0) != 0)
            return -1;
    } else {
        attributes = 1;
    }
    if (HOST_FILE_MOVE(destination, 0, source, 0) != 0 ||
        HOST_SET_ATTRIBUTES(attributes, destination, 0) != 0)
        return -1;
    return 0;
}

static int32_t facil_serialize(const uint8_t *ids, uint32_t count)
{
    struct fs98_host_file *file;
    uint32_t header[3] = { 0, 0, 0 };
    uint32_t wrote;
    uint32_t position;
    char temporary[0x104];
    char destination[0x104];

    FS98_HOST_MEMSET(HOST)(LAST_OFFSETS, 0, 0x14);
    FS98_HOST_LOADSTRING(HOST)(0x9101, destination, sizeof(destination));
    if (HOST_TEMP(temporary, 0) != 0 ||
        HOST_FILE_CREATE(&file, temporary, 0) != 0 ||
        HOST_FILE_WRITE(file, header, sizeof(header), &wrote) != 0 ||
        wrote != sizeof(header))
        return -1;
    if (facil_backfill(file, 8, count) != 0 ||
        HOST_FILE_WRITE(file, ids, count * 16, &wrote) != 0 ||
        wrote != count * 16)
        return -1;
    if (HOST_FILE_SIZE(file, &position) != 0 ||
        facil_backfill(file, 4, position) != 0 ||
        facil_write_tree(file, ROOT_TREE, 0, 4) != 0 ||
        HOST_FILE_SIZE(file, &position) != 0 ||
        facil_backfill(file, 0, position) != 0 ||
        FS98_HOST_FILE_CLOSE(HOST)(file) != 0)
        return -1;
    return facil_replace_index(destination, temporary);
}

static int32_t facil_build_index(void)
{
    struct fs98_host_list blocks;
    uint8_t *ids = NULL;
    uint32_t count = 0;
    uint32_t index;
    uint32_t start;
    int32_t result = 0;
    void *alert = NULL;

    UTIL_ALERT(&alert, 1, 0xf570, 0, 0, 0, 0, 0);
    start = FS98_HOST_TICKS(HOST)();
    blocks.count = 0;
    blocks.capacity = 0;
    blocks.grow = 0x20;
    blocks.elem_size = 4;
    blocks.iterating = 0;
    blocks.pending = 0;
    blocks.data = NULL;
    if (facil_reset_tree() != 0 || !FE_LIST(&ids, &count))
        result = -1;
    if (result == 0) {
        facil_sort_ids(ids, count);
        facil_unique_ids(ids, &count);
        for (index = count; index != 0; index--) {
            uint8_t *block = NULL;
            uint8_t **stored;

            ACTIVE_ID = ids + (index - 1) * 16;
            if (!FE_OPEN(ACTIVE_ID, &block) || facil_parse_block(block) != 0) {
                result = -1;
                break;
            }
            stored = (uint8_t **)FS98_HOST_LIST_ADD(HOST)(&blocks);
            *stored = block;
        }
    }
    if (result == 0 && facil_serialize(ids, count) != 0)
        result = -1;
    if (FS98_HOST_FREE(HOST)((void **)&ids) != 0)
        result = -1;
    for (index = (uint32_t)blocks.count; index != 0; index--)
        if (FS98_HOST_FREE(HOST)(
                (void **)((uint8_t *)blocks.data + (index - 1) * 4)) != 0)
            result = -1;
    if (blocks.data != NULL && FS98_HOST_FREE(HOST)(&blocks.data) != 0)
        result = -1;
    if (facil_reset_tree() != 0)
        result = -1;
    while (FS98_HOST_TICKS(HOST)() < start + 0xfa0)
        Sleep(0);
    if (alert != NULL)
        UTIL_ALERT_CLOSE(&alert);
    return result;
}

/* ---- the mapped cache: .text+0x5680 and +0x57e0 -------------------------
 *
 * +0x18 maps INDEX_FILE on first use and leaves it mapped on success; +0x1c
 * and +0x20 read through that mapping without mapping it themselves.  Retail
 * keeps three copies of the view base.  Node offsets are file offsets, and a
 * zero offset reads the header, exactly as in retail. */

#define MAP_FIRST            D32(0x2b50)
#define MAP_HEADER           D32(0x2b54)
#define MAP_VIEW             D32(0x2b58)
#define MAP_EMPTY            D32(0x2b5c)
#define QUEUE                DPTR(0x2d04, struct facil_queue)
#define MAPPED               D32(0x2d08)
#define MAP_FILE             D32(0x2d0c)
#define MAP_MAPPING          D32(0x2d10)

#define VIEW                 ((uint8_t *)(uintptr_t)MAP_VIEW)
#define NODE(offset, field)  (*(facil_u32 *)(VIEW + (offset) + (field)))
#define NODE_PARENT          0x00
#define NODE_SIBLING         0x04
#define NODE_CHILD           0x08
#define NODE_KIND            0x0c
#define NODE_FLAGS           0x14
#define NODE_NAME(offset)    ((char *)VIEW + (offset) + 0x20)
#define LEVEL(kind)          (((kind) - 0x10000) >> 16)
#define LEVEL_BITS(kind)     (((kind) - 0x10000) & 0xffff0000)

typedef int32_t (FS98_STDCALL *facil_basedir_fn)(char *);
#define HOST_BASEDIR          SLOT(HOST, 0x130, facil_basedir_fn)

/* A caller's filter: a kind (0 in the low word matches the whole level) and
 * an optional name. */
struct facil_filter {
    uint32_t kind;
    const char *name;
};

struct facil_filter_list {
    uint32_t count;
    struct facil_filter *entries;
};

/* What both query services return: 12-byte records, names owned by the
 * caller. */
struct facil_result {
    char *name;
    uint32_t kind;
    uint32_t node;
};

struct facil_results {
    uint32_t count;
    struct facil_result *records;
};

/* A C++ priority queue of result pointers, 1-based: `data` is kept one slot
 * below the host allocation. */
struct facil_queue {
    uint32_t capacity;
    uint32_t grow;
    uint32_t count;
    struct facil_result **data;
    uint32_t flags;
};

typedef char (*facil_query_check)[
    sizeof(struct facil_filter) == 8 &&
    sizeof(struct facil_result) == 0xc &&
    sizeof(struct facil_queue) == 0x14 ? 1 : -1];

/* Retail allocates the queue with MSVCRT's operator new/delete.  mingw's
 * import library does not carry those mangled names, so they are bound by
 * name on first use; the routines are the ones retail calls. */
typedef void *(__cdecl *facil_new_fn)(uint32_t);
typedef void (__cdecl *facil_delete_fn)(void *);
static facil_new_fn facil_new;
static facil_delete_fn facil_delete;

static void facil_bind_crt(void)
{
    HMODULE crt;

    if (facil_new != NULL)
        return;
    crt = LoadLibraryA("MSVCRT.dll");
    facil_new = (facil_new_fn)(void *)GetProcAddress(crt, "??2@YAPAXI@Z");
    facil_delete = (facil_delete_fn)(void *)GetProcAddress(crt,
                                                     "??3@YAXPAX@Z");
}

/* .text+0x5680 */
static int32_t facil_map(void)
{
    char directory[0x108];
    char name[0x100];
    char path[0x104];
    HANDLE file;
    HANDLE mapping;
    uint8_t *header;

    if (MAPPED == 0) {
        FS98_HOST_LOADSTRING(HOST)(0x9101, name, 0x100);
        if (HOST_BASEDIR(directory) != 0)
            return -1;
        if (FS98_HOST_PATH_JOIN(HOST)(path, NULL, directory, name) != 0)
            return -1;
        file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_FLAG_RANDOM_ACCESS | FILE_ATTRIBUTE_NORMAL,
                           NULL);
        MAP_FILE = (uint32_t)(uintptr_t)file;
        if (file == INVALID_HANDLE_VALUE)
            return -1;
        mapping = CreateFileMappingA(file, NULL, PAGE_READWRITE, 0, 0, NULL);
        MAP_MAPPING = (uint32_t)(uintptr_t)mapping;
        if (mapping == NULL) {
            CloseHandle((HANDLE)(uintptr_t)MAP_FILE);
            return -1;
        }
        MAP_VIEW = (uint32_t)(uintptr_t)MapViewOfFile(mapping, FILE_MAP_READ,
                                                      0, 0, 0);
        /* Retail closes both handles on a failed view but still marks the
         * cache mapped and reads its header through the null view. */
        if (MAP_VIEW == 0) {
            CloseHandle((HANDLE)(uintptr_t)MAP_MAPPING);
            CloseHandle((HANDLE)(uintptr_t)MAP_FILE);
        }
        MAPPED = 1;
        MAP_HEADER = MAP_VIEW;
        MAP_FIRST = MAP_VIEW;
    }
    header = (uint8_t *)(uintptr_t)MAP_HEADER;
    MAP_EMPTY = *(facil_u32 *)header == *(facil_u32 *)(header + 4);
    return 0;
}

/* .text+0x57e0.  The three view copies are left stale. */
static int32_t facil_unmap(void)
{
    if (MAPPED != 0) {
        if (!UnmapViewOfFile((void *)(uintptr_t)MAP_VIEW))
            return -1;
        if (!CloseHandle((HANDLE)(uintptr_t)MAP_MAPPING))
            return -1;
        if (!CloseHandle((HANDLE)(uintptr_t)MAP_FILE))
            return -1;
        MAPPED = 0;
    }
    return 0;
}

/* ---- the result queue: .text+0x5840..+0x5dd0 and +0x6910 ---------------- */

/* .text+0x5840.  Bits other than 0 and 1 of `flags` keep whatever operator
 * new returned. */
static int32_t facil_queue_create(void)
{
    struct facil_queue *queue;
    uint32_t flags;

    facil_bind_crt();
    queue = (struct facil_queue *)facil_new(0x14);
    if (queue != NULL) {
        flags = queue->flags;
        queue->capacity = 0x200;
        queue->grow = 0x80;
        queue->count = 0;
        queue->data = NULL;
        queue->flags = (flags & ~2u) | 1;
        FS98_HOST_ALLOC(HOST)(1, 0x800, (void **)&queue->data, 0);
        queue->data--;
    }
    QUEUE = queue;
    /* Retail then clears bit 1 when bit 0, sign-extended, equals 1.  That
     * never holds, so only the read (which faults on a failed new) remains. */
    flags = *(volatile uint32_t *)&queue->flags;
    (void)flags;
    return 0;
}

/* .text+0x58e0.  Bit 1 (owning) is always clear, so records are never deleted
 * here; the pointer is left stale. */
static int32_t facil_queue_destroy(void)
{
    struct facil_queue *queue = QUEUE;
    uint32_t count;

    if (queue != NULL) {
        if (queue->flags & 1) {
            queue->data++;
            if (queue->flags & 2) {
                for (;;) {
                    count = queue->count;
                    queue->count = count - 1;
                    if (count == 0)
                        break;
                    facil_delete(queue->data[count - 1]);
                }
            }
            FS98_HOST_FREE(HOST)((void **)&queue->data);
        }
        facil_delete(queue);
    }
    return 0;
}

/* .text+0x5dd0 */
static int facil_name_greater(const struct facil_result *left,
                              const struct facil_result *right)
{
    return FS98_HOST_STRICMP(HOST)(left->name, right->name) > 0;
}

/* .text+0x6910 */
static void facil_sift_down(struct facil_queue *queue, uint32_t index)
{
    for (;;) {
        uint32_t left = index * 2;
        uint32_t right = index * 2 + 1;
        uint32_t best = index;
        struct facil_result *swap;

        if (left <= queue->count &&
            facil_name_greater(queue->data[index], queue->data[left]))
            best = left;
        if (right <= queue->count &&
            facil_name_greater(queue->data[best], queue->data[right]))
            best = right;
        if (best == index)
            return;
        swap = queue->data[index];
        queue->data[index] = queue->data[best];
        queue->data[best] = swap;
        index = best;
    }
}

/* Inlined three times in .text+0x5aa0. */
static struct facil_result *facil_queue_pop(struct facil_queue *queue)
{
    struct facil_result *top;
    struct facil_result *swap;
    uint32_t best;

    if (queue->count == 0)
        return NULL;
    top = queue->data[1];
    queue->data[1] = queue->data[queue->count];
    queue->count--;
    best = 1;
    if (queue->count >= 2 &&
        facil_name_greater(queue->data[1], queue->data[2]))
        best = 2;
    if (queue->count >= 3 &&
        facil_name_greater(queue->data[best], queue->data[3]))
        best = 3;
    if (best != 1) {
        swap = queue->data[1];
        queue->data[1] = queue->data[best];
        queue->data[best] = swap;
        facil_sift_down(queue, best);
    }
    return top;
}

/* .text+0x5960 -- copy a node's name, kind and offset into the queue. */
static int32_t facil_queue_insert(uint32_t node)
{
    struct facil_result *record;
    struct facil_queue *queue;
    struct facil_result *parent;
    uint32_t index;
    uint32_t above;

    if (FS98_HOST_ALLOC(HOST)(1, 0xc, (void **)&record, 0) != 0)
        return -1;
    if (FS98_HOST_ALLOC(HOST)(1, FS98_HOST_STRLEN(HOST)(NODE_NAME(node)) + 1,
                              (void **)&record->name, 0) != 0)
        return -1;
    FS98_HOST_STRCPY(HOST)(record->name, NODE_NAME(node));
    record->node = node;
    record->kind = NODE(node, NODE_KIND);
    queue = QUEUE;
    if (queue->count == queue->capacity) {
        queue->capacity = queue->grow + queue->capacity;
        queue->data++;
        FS98_HOST_REALLOC(HOST)(1, queue->capacity << 2,
                                (void **)&queue->data);
        queue->data--;
    }
    index = ++queue->count;
    while (index > 1) {
        above = index >> 1;
        parent = queue->data[above];
        if (FS98_HOST_STRICMP(HOST)(record->name, parent->name) > 0)
            break;
        queue->data[index] = parent;
        index = above;
    }
    queue->data[index] = record;
    return 0;
}

/* .text+0x5aa0 -- empty the queue into `out` in name order.  With `dedupe`,
 * a record whose name equals the last one kept (case-insensitively) is
 * dropped, name and all. */
static int32_t facil_queue_drain(struct facil_results *out, uint32_t dedupe)
{
    struct facil_result *record;
    uint32_t written = 0;
    uint32_t offset;

    out->count = 0;
    if (QUEUE->count == 0) {
        out->records = NULL;
        out->count = 0;
        return 0;
    }
    if (FS98_HOST_ALLOC(HOST)(1, QUEUE->count * 0xc, (void **)&out->records,
                              0) != 0)
        return -1;
    record = facil_queue_pop(QUEUE);
    if (record != NULL) {
        offset = 0;
        do {
            if (FS98_HOST_MEMMOVE(HOST)((uint8_t *)out->records + offset,
                                        record, 0xc) != 0)
                return -1;
            if (FS98_HOST_FREE(HOST)((void **)&record) != 0)
                return -1;
            record = facil_queue_pop(QUEUE);
            written++;
            offset += 0xc;
            if (dedupe == 0)
                continue;
            while (record != NULL &&
                   FS98_HOST_STRICMP(HOST)(
                       record->name,
                       *(char **)((uint8_t *)out->records + offset - 0xc))
                       == 0) {
                if (FS98_HOST_FREE(HOST)((void **)record) != 0 ||
                    FS98_HOST_FREE(HOST)((void **)&record) != 0)
                    return -1;
                record = facil_queue_pop(QUEUE);
            }
        } while (record != NULL);
    }
    out->count = written;
    return 0;
}

/* ---- filters: .text+0x5e00..+0x6510 ------------------------------------- */

/* .text+0x5e00 -- descend the first-child chain to the filter's level. */
static uint32_t facil_first_at_level(uint32_t kind)
{
    uint32_t node = *(facil_u32 *)((uint8_t *)(uintptr_t)MAP_HEADER + 4);
    uint32_t want;

    if (node != 0) {
        want = LEVEL_BITS(kind);
        do {
            if (LEVEL_BITS(NODE(node, NODE_KIND)) >= want)
                break;
            node = NODE(node, NODE_CHILD);
        } while (node != 0);
    }
    return node;
}

/* .text+0x61a0 -- kind level, exact kind when the low word is set, and the
 * whole name case-insensitively when the entry has one. */
static uint32_t facil_entry_matches(const struct facil_filter *entry,
                                    uint32_t node)
{
    uint32_t node_kind = NODE(node, NODE_KIND);
    uint32_t kind = entry->kind;

    if (((kind - 0x10000) ^ (node_kind - 0x10000)) & 0xffff0000)
        return 0;
    if ((kind & 0xffff) != 0 && kind != node_kind)
        return 0;
    if (entry->name != NULL && FS98_HOST_STRLEN(HOST)(entry->name) != 0 &&
        FS98_HOST_STRICMP(HOST)(NODE_NAME(node), entry->name) != 0)
        return 0;
    return 1;
}

/* .text+0x60d0 -- as above, but the name is a case-sensitive prefix. */
static uint32_t facil_filter_matches(const struct facil_filter *filter,
                                     uint32_t node)
{
    uint32_t node_kind = NODE(node, NODE_KIND);
    uint32_t kind = filter->kind;
    uint32_t length;

    if (((kind - 0x10000) ^ (node_kind - 0x10000)) & 0xffff0000)
        return 0;
    if ((kind & 0xffff) != 0 && kind != node_kind)
        return 0;
    if (filter->name == NULL)
        return 1;
    length = FS98_HOST_STRLEN(HOST)(filter->name);
    if (length == 0)
        return 1;
    if (FS98_HOST_STRLEN(HOST)(NODE_NAME(node)) < length)
        return 0;
    return FS98_HOST_MEMCMP(HOST)(NODE_NAME(node), filter->name,
                                  FS98_HOST_STRLEN(HOST)(filter->name)) == 0;
}

/* .text+0x5e90 -- every filter entry above the node's level must match the
 * node's ancestor at that level. */
static uint32_t facil_ancestors_match(const struct facil_filter_list *list,
                                      uint32_t node)
{
    uint32_t level = LEVEL(NODE(node, NODE_KIND));
    uint32_t index = 0;
    uint32_t remaining;
    uint32_t entry_level;
    uint32_t node_level;
    const struct facil_filter *entry;

    if (list->count != 0) {
        while (LEVEL(list->entries[index].kind) < level) {
            index++;
            if (index >= list->count)
                break;
        }
    }
    if (index == 0) {
        entry_level = LEVEL(list->entries[0].kind);
        if (entry_level > level)
            return 1;
        if (entry_level == level)
            return facil_entry_matches(&list->entries[0], node);
    }
    remaining = index;
    entry = &list->entries[index - 1];
    for (;;) {
        entry_level = LEVEL(entry->kind);
        node_level = LEVEL(NODE(node, NODE_KIND));
        while (node_level > entry_level) {
            node = NODE(node, NODE_PARENT);
            node_level = LEVEL(NODE(node, NODE_KIND));
        }
        if (!facil_entry_matches(entry, node))
            return 0;
        entry--;
        if (--remaining == 0)
            return 1;
    }
}

/* .text+0x6010 -- does some node in this sibling run (or below it) match
 * entries[index..]?  Siblings are followed only while their parent is
 * `limit`; the top-level call passes 0, so it checks just the one node. */
static uint32_t facil_subtree_match(const struct facil_filter_list *list,
                                    uint32_t limit, uint32_t node,
                                    uint32_t index)
{
    uint32_t entry_level;

    if (node == 0)
        return 0;
    entry_level = LEVEL(list->entries[index].kind);
    for (;;) {
        if (LEVEL(NODE(node, NODE_KIND)) == entry_level) {
            if (facil_entry_matches(&list->entries[index], node)) {
                if (index == list->count - 1)
                    return 1;
                if (facil_subtree_match(list, node, NODE(node, NODE_CHILD),
                                        index + 1))
                    return 1;
            }
        } else if (facil_subtree_match(list, node, NODE(node, NODE_CHILD),
                                       index)) {
            return 1;
        }
        node = NODE(node, NODE_SIBLING);
        if (node == 0 || NODE(node, NODE_PARENT) == 0 ||
            NODE(node, NODE_PARENT) != limit)
            return 0;
    }
}

/* .text+0x5f80 -- the filter entries at and below the node's level must be
 * satisfied by the node or its descendants. */
static uint32_t facil_descendants_match(const struct facil_filter_list *list,
                                        uint32_t node)
{
    uint32_t level = LEVEL(NODE(node, NODE_KIND));
    uint32_t count = list->count;
    uint32_t index = 0;
    uint32_t result;

    if (count != 0) {
        while (LEVEL(list->entries[index].kind) < level) {
            index++;
            if (index >= count)
                break;
        }
    }
    if (index == count)
        return 1;
    if (LEVEL(list->entries[index].kind) == level)
        result = facil_subtree_match(list, 0, node, index);
    else
        result = facil_subtree_match(list, node, NODE(node, NODE_CHILD),
                                     index);
    return result != 0 ? 1 : 0;
}

/* .text+0x5e50.  Both halves always run. */
static uint32_t facil_matches_filters(const struct facil_filter_list *list,
                                      uint32_t node)
{
    if (list != NULL && list->count != 0)
        return facil_ancestors_match(list, node) &
               facil_descendants_match(list, node);
    return 1;
}

/* .text+0x6440 */
static uint32_t facil_entry_unnamed(const struct facil_filter *entry)
{
    if (entry->name == NULL)
        return 1;
    return FS98_HOST_STRLEN(HOST)(entry->name) == 0;
}

/* .text+0x6470 */
static int32_t facil_list_copy(struct facil_filter_list *destination,
                               const struct facil_filter_list *source)
{
    if (destination != NULL && source != NULL) {
        destination->count = source->count;
        if (FS98_HOST_ALLOC(HOST)(1, source->count << 3,
                                  (void **)&destination->entries, 0) != 0)
            return -1;
        if (FS98_HOST_MEMMOVE(HOST)(destination->entries, source->entries,
                                    source->count << 3) != 0)
            return -1;
    }
    return 0;
}

/* .text+0x6510 */
static int32_t FS98_STDCALL facil_filter_compare(const void *left,
                                                 const void *right)
{
    uint32_t a = *(const facil_u32 *)left;
    uint32_t b = *(const facil_u32 *)right;

    if (a > b)
        return 1;
    return a != b ? -1 : 0;
}

/* .text+0x64e0 */
static int32_t facil_list_sort(struct facil_filter_list *list)
{
    FS98_HOST_QSORT(HOST)(list->entries, list->count, 8,
                          facil_filter_compare);
    return 0;
}

/* .text+0x6370 -- replace `list` with the named entries of `source`, in
 * source order.  The sorted copy the caller just made is freed unused. */
static int32_t facil_named_filters(const struct facil_filter_list *source,
                                   const struct facil_filter *unused,
                                   struct facil_filter_list *list)
{
    struct fs98_host_list named = { 0, 0, 0x20, 8, 0, 0, NULL };
    uint32_t index;
    void *slot;

    (void)unused;
    list->count = 0;
    if (source != NULL) {
        for (index = 0; index < source->count; index++) {
            if (facil_entry_unnamed(&source->entries[index]))
                continue;
            slot = FS98_HOST_LIST_ADD(HOST)(&named);
            if (slot == NULL ||
                FS98_HOST_MEMMOVE(HOST)(slot, &source->entries[index], 8) != 0)
                return -1;
        }
    }
    if (list->entries != NULL)
        FS98_HOST_FREE(HOST)((void **)&list->entries);
    list->entries = (struct facil_filter *)named.data;
    list->count = (uint32_t)named.count;
    return 0;
}

/* ---- +0x1c's helpers: .text+0x6540..+0x6770 ----------------------------- */

/* .text+0x6770 -- step over a run of flag-2 nodes (an airport and the name
 * record serialized after it).  Returns 1 when `target` is in the run. */
static uint32_t facil_group_step(uint32_t node, uint32_t target,
                                 uint32_t *next)
{
    if (node != target) {
        do {
            if (!(VIEW[node + NODE_FLAGS] & 2)) {
                *next = NODE(node, NODE_SIBLING);
                return 0;
            }
            node = NODE(node, NODE_SIBLING);
        } while (node != target);
    }
    *next = node;
    return 1;
}

/* .text+0x6710 -- the first node of the run containing `node`. */
static uint32_t facil_group_start(uint32_t node)
{
    uint32_t start;
    uint32_t next;

    if (LEVEL_BITS(NODE(node, NODE_KIND)) == 0)
        return *(facil_u32 *)((uint8_t *)(uintptr_t)MAP_HEADER + 4);
    start = NODE(NODE(node, NODE_PARENT), NODE_CHILD);
    while (start < node) {
        if (facil_group_step(start, node, &next))
            break;
        start = next;
    }
    return start;
}

/* .text+0x6610 -- climb from `start` to the entry's level and take the last
 * matching node of that run.  A second match leaks the first name. */
static int32_t facil_resolve(uint32_t start, const struct facil_filter *entry,
                             struct facil_result *record)
{
    uint32_t node = start;
    uint32_t want;

    record->name = NULL;
    record->kind = 0;
    record->node = 0;
    want = LEVEL_BITS(entry->kind);
    if (want > LEVEL_BITS(NODE(node, NODE_KIND)) || node == 0)
        return 0;
    while (LEVEL_BITS(NODE(node, NODE_KIND)) > want) {
        node = NODE(node, NODE_PARENT);
        if (node == 0)
            return 0;
    }
    for (;;) {
        if (!(VIEW[node + NODE_FLAGS] & 1) &&
            facil_entry_matches(entry, node)) {
            if (FS98_HOST_ALLOC(HOST)(
                    1, FS98_HOST_STRLEN(HOST)(NODE_NAME(node)) + 1,
                    (void **)&record->name, 0) != 0)
                return -1;
            FS98_HOST_STRCPY(HOST)(record->name, NODE_NAME(node));
            record->node = node;
            record->kind = NODE(node, NODE_KIND);
        }
        if (!(VIEW[node + NODE_FLAGS] & 2))
            return 0;
        node = NODE(node, NODE_SIBLING);
        if (node == 0)
            return 0;
    }
}

/* .text+0x6540 */
static int32_t facil_collect(uint32_t start,
                             const struct facil_filter_list *filters,
                             struct facil_results *out)
{
    struct fs98_host_list results = { 0, 0, 0x20, 0xc, 0, 0, NULL };
    struct facil_result record;
    uint32_t written = 0;
    uint32_t index;
    void *slot;

    out->count = 0;
    for (index = 0; index < filters->count; index++) {
        if (facil_resolve(start, &filters->entries[index], &record) != 0)
            return -1;
        if (record.name == NULL)
            continue;
        slot = FS98_HOST_LIST_ADD(HOST)(&results);
        if (slot == NULL ||
            FS98_HOST_MEMMOVE(HOST)(slot, &record, 0xc) != 0)
            return -1;
        written++;
    }
    out->count = written;
    out->records = (struct facil_result *)results.data;
    return 0;
}

/* .text+0x5420 -- Linkage +0x18: every node at `filter`'s level whose name
 * starts with filter's name and which fits the named entries of `filters`,
 * in name order.  Every failure after the zeroing unmaps the cache and, if
 * that succeeds, returns 0 -- retail's error result is the unmap's.  Success
 * leaves the cache mapped for +0x1c and +0x20; failure also leaks the queue
 * and the filter copy. */
static uint32_t FS98_STDCALL facil_search(
    const struct facil_filter_list *filters, const struct facil_filter *filter,
    struct facil_results *out, uint32_t dedupe)
{
    struct facil_filter_list named = { 0, NULL };
    uint32_t node;

    out->count = 0;
    out->records = NULL;
    if (facil_map() != 0 || MAP_EMPTY != 0)
        goto fail;
    if (facil_queue_create() != 0)
        return 0xffffffffu;
    node = facil_first_at_level(filter->kind);
    if (facil_list_copy(&named, filters) != 0 ||
        facil_list_sort(&named) != 0 ||
        facil_named_filters(filters, filter, &named) != 0)
        goto fail;
    for (; node != 0; node = NODE(node, NODE_SIBLING))
        if (facil_matches_filters(&named, node) &&
            facil_filter_matches(filter, node) &&
            facil_queue_insert(node) != 0)
            goto fail;
    if (facil_queue_drain(out, dedupe) != 0 ||
        facil_queue_destroy() != 0 ||
        FS98_HOST_FREE(HOST)((void **)&named.entries) != 0)
        goto fail;
    return 0;
fail:
    return facil_unmap() != 0 ? 0xffffffffu : 0;
}

/* .text+0x55c0 -- Linkage +0x1c: for each entry of `filters`, in caller
 * order, the selected node's ancestor-or-self that matches it.  Reads the
 * mapping +0x18 left behind.  The sorted copy is made and freed unused. */
static uint32_t FS98_STDCALL facil_describe(
    uint32_t node, const struct facil_filter_list *filters,
    struct facil_results *out)
{
    struct facil_filter_list sorted = { 0, NULL };

    out->count = 0;
    out->records = NULL;
    if (facil_list_copy(&sorted, filters) != 0 ||
        facil_list_sort(&sorted) != 0)
        return 0xffffffffu;
    if (facil_collect(facil_group_start(node), filters, out) != 0)
        return 0xffffffffu;
    return FS98_HOST_FREE(HOST)((void **)&sorted.entries) != 0 ?
           0xffffffffu : 0;
}

static uint32_t FS98_STDCALL facil_rebuild(void)
{
    uint8_t *old_ids = NULL;
    uint8_t *new_ids = NULL;
    uint32_t old_count = 0;
    uint32_t new_count = 0;
    int32_t result = 0;

    if (facil_unmap() != 0)
        return 0xffffffffu;
    if (facil_read_old_ids(&old_ids, &old_count) != 0)
        return 0xffffffffu;
    if (!FE_LIST(&new_ids, &new_count))
        return 0xffffffffu;
    facil_sort_ids(new_ids, new_count);
    facil_unique_ids(new_ids, &new_count);
    if (facil_index_changed(old_ids, old_count, new_ids, new_count) &&
        facil_build_index() != 0)
        result = -1;
    if (FS98_HOST_FREE(HOST)((void **)&old_ids) != 0 ||
        FS98_HOST_FREE(HOST)((void **)&new_ids) != 0)
        result = -1;
    return (uint32_t)result;
}

/* .text+0x5540 -- copy one serialized cache record for a caller that already
 * selected its file offset.  The search/list services own the mapping's
 * lifetime; this slot deliberately does not open INDEX_FILE itself. */
static uint32_t FS98_STDCALL facil_copy_record(uint32_t offset,
                                               uint8_t **record,
                                               uint32_t *size)
{
    uint8_t *source = (uint8_t *)(uintptr_t)D32(0x2b58) + offset;
    uint32_t record_size = *(facil_u32 *)(source + 0x10) + 0x14;

    *record = NULL;
    *size = 0;
    if (offset != 0) {
        if (FS98_HOST_ALLOC(HOST)(1, record_size, (void **)record, 0) != 0)
            return 0xffffffffu;
        if (FS98_HOST_MEMMOVE(HOST)(*record, source, record_size) != 0)
            return 0xffffffffu;
        *size = record_size;
    }
    return 0;
}

static uint32_t FS98_STDCALL facil_init(void)
{
    FS98_HOST_CFG_STRING(HOST)((const char *)D + 0x4f0,
                               (const char *)D + 0x53c,
                               (char *)D + 0x2b60, 0x64);
    FS98_HOST_CFG_STRING(HOST)((const char *)D + 0x4f0,
                               (const char *)D + 0x534,
                               (char *)D + 0x2bc8, 0x64);
    FS98_HOST_CFG_STRING(HOST)((const char *)D + 0x4f0,
                               (const char *)D + 0x52c,
                               (char *)D + 0x2c30, 0x64);
    FS98_HOST_CFG_STRING(HOST)((const char *)D + 0x4f0,
                               (const char *)D + 0x524,
                               (char *)D + 0x2c98, 0x64);
    HOST_CFG_INT((const char *)D + 0x4f0, (const char *)D + 0x518,
                 (int32_t *)(D + 0x4ec));
    HOST_CFG_INT((const char *)D + 0x4f0, (const char *)D + 0x4fc,
                 (int32_t *)(D + 0x2d00));
    return 0;
}

static uint32_t FS98_STDCALL facil_deinit(void)
{
    if (facil_unmap() != 0)
        return 0xffffffffu;
    HOST_CFG_PUT_STRING((const char *)D + 0x4f0, (const char *)D + 0x53c,
                        (const char *)D + 0x2b60);
    HOST_CFG_PUT_STRING((const char *)D + 0x4f0, (const char *)D + 0x534,
                        (const char *)D + 0x2bc8);
    HOST_CFG_PUT_STRING((const char *)D + 0x4f0, (const char *)D + 0x52c,
                        (const char *)D + 0x2c30);
    HOST_CFG_PUT_STRING((const char *)D + 0x4f0, (const char *)D + 0x524,
                        (const char *)D + 0x2c98);
    HOST_CFG_PUT_INT((const char *)D + 0x4f0, (const char *)D + 0x518,
                     (int32_t)D32(0x4ec));
    return 0;
}

static uint32_t FS98_STDCALL facil_unimplemented(void)
{
    return 0xffffffffu;
}

struct facil_image facil_image = {
    {
        { FS98_MOD_HOST,    NULL },
        { FS98_MOD_GLOBAL,  NULL },
        { FS98_MOD_CHAIN,   NULL },
        { FS98_MOD_FE,      NULL },
        { FS98_MOD_MAIN,    NULL },
        { FS98_MOD_NUMERIC, NULL },
        { FS98_MOD_UTIL,    NULL },
        { 0,                NULL }
    },
    { FS98_MOD_FACIL, NULL, NULL, 0, 0, NULL, { NULL } },
    {
#include "facil_data.inc"
    }
};

__asm__(".globl _ImportTable\n\t.set _ImportTable, _facil_image\n\t"
        ".globl _Linkage\n\t.set _Linkage, _facil_image + 0x40");

static void facil_install_pointers(void)
{
#define FACIL_RELOCS
#define FACIL_REL_DATA(offset, target) \
    D32(offset) = (uint32_t)(uintptr_t)(D + (target));
#define FACIL_REL_TEXT(offset, target) \
    D32(offset) = (uint32_t)(uintptr_t)facil_unimplemented;
#include "facil_data.inc"
#undef FACIL_REL_TEXT
#undef FACIL_REL_DATA
#undef FACIL_RELOCS

    facil_image.linkage.init = facil_init;
    facil_image.linkage.deinit = facil_deinit;
    facil_image.linkage.slots[0] = (facil_fn)facil_search;
    facil_image.linkage.slots[1] = (facil_fn)facil_describe;
    facil_image.linkage.slots[2] = (facil_fn)facil_copy_record;
    facil_image.linkage.slots[3] = (facil_fn)facil_unimplemented;
    facil_image.linkage.slots[4] = (facil_fn)facil_unimplemented;
    facil_image.linkage.slots[5] = (facil_fn)facil_rebuild;
    facil_image.linkage.slots[6] = (facil_fn)facil_unimplemented;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        facil_install_pointers();
    }
    return TRUE;
}
