#include "string.h"
#include <stdalign.h>

Sv sv_create_from_cstr(const char *str) {
  Sv sv;
  sv.count = strlen(str);
  sv.data = str;
  return sv;
}

bool sv_equal(Sv a, Sv b) {
  if (a.count != b.count) {
    return false;
  } else {
    return memcmp(a.data, b.data, a.count) == 0;
  }
}

Sv sv2_from_parts(const char *data, size_t count) {
  Sv sv;
  sv.count = count;
  sv.data = data;
  return sv;
}

Sv sv2_chop_by_delim(Sv *sv, char delim) {
  size_t i = 0;
  while (i < sv->count && sv->data[i] != delim) {
    i += 1;
  }

  Sv result = sv2_from_parts(sv->data, i);

  if (i < sv->count) {
    sv->count -= i + 1;
    sv->data += i + 1;
  } else {
    sv->count -= i;
    sv->data += i;
  }

  return result;
}
char *sv_to_cstr_arena(memory_arena *arena, Sv sv) {
  char *dst = (char *)arena_alloc(arena, sv.count + 1, alignof(char));
  if (!dst)
    return NULL;
  memcpy(dst, sv.data, sv.count);
  dst[sv.count] = '\0';
  return dst;
}
