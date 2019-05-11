static int
pack_arrayv(const int xmin, const hydroparam_t H, hydrovar_t * Hv, real_t *buffer);
static int
unpack_arrayv(const int xmin, const hydroparam_t H, hydrovar_t * Hv, real_t *buffer);
static int
pack_arrayh(const int xmin, const hydroparam_t H, hydrovar_t * Hv, real_t *buffer);
static int
unpack_arrayh(const int xmin, const hydroparam_t H, hydrovar_t * Hv, real_t *buffer);

int
pack_arrayv(const int xmin, const hydroparam_t H, hydrovar_t * Hv, real_t *buffer) {
  int ivar, i, j, p = 0;
  for (ivar = 0; ivar < H.nvar; ivar++) {
    for (j = 0; j < H.nyt; j++) {
      // #warning "GATHER to vectorize ?"
      for (i = xmin; i < xmin + ExtraLayer; i++) {
        buffer[p++] = Hv->uold[IHv(i, j, ivar)];
      }
    }
  }
  return p;
}

int
unpack_arrayv(const int xmin, const hydroparam_t H, hydrovar_t * Hv, real_t *buffer) {
  int ivar, i, j, p = 0;
  for (ivar = 0; ivar < H.nvar; ivar++) {
    for (j = 0; j < H.nyt; j++) {
      // #warning "SCATTER to vectorize ?"
      for (i = xmin; i < xmin + ExtraLayer; i++) {
        Hv->uold[IHv(i, j, ivar)] = buffer[p++];
      }
    }
  }
  return p;
}

int
pack_arrayh(const int ymin, const hydroparam_t H, hydrovar_t * Hv, real_t *buffer) {
  int ivar, i, j, p = 0;
  for (ivar = 0; ivar < H.nvar; ivar++) {
    for (j = ymin; j < ymin + ExtraLayer; j++) {
      // #warning "GATHER to vectorize ?"
      // #pragma simd
      for (i = 0; i < H.nxt; i++) {
        buffer[p++] = Hv->uold[IHv(i, j, ivar)];
      }
    }
  }
  return p;
}

int
unpack_arrayh(const int ymin, const hydroparam_t H, hydrovar_t * Hv, real_t *buffer) {
  int ivar, i, j, p = 0;
  for (ivar = 0; ivar < H.nvar; ivar++) {
    for (j = ymin; j < ymin + ExtraLayer; j++) {
      // #warning "SCATTER to vectorize ?"
      for (i = 0; i < H.nxt; i++) {
        Hv->uold[IHv(i, j, ivar)] = buffer[p++];
      }
    }
  }
  return p;
}

#define VALPERLINE 11
int
print_bufferh(FILE * fic, const int ymin, const hydroparam_t H, hydrovar_t * Hv, real_t *buffer) {
  int ivar, i, j, p = 0, nbr = 1;
  for (ivar = 3; ivar < H.nvar; ivar++) {
    fprintf(fic, "BufferH v=%d\n", ivar);
    for (j = ymin; j < ymin + ExtraLayer; j++) {
      for (i = 0; i < H.nxt; i++) {
        fprintf(fic, "%13.6le ", buffer[p++]);
        nbr++;
        if (nbr == VALPERLINE) {
          fprintf(fic, "\n");
          nbr = 1;
        }
      }
    }
    if (nbr != 1)
      fprintf(fic, "\n");
  }
  return p;
}
