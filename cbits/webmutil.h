typedef void *Context;
typedef char *Buffer;

Context allocVP8(int width, int height);
void     freeVP8(Context ctx);
Buffer  rawFrameBuffer(Context ctx);
Buffer  encodeFrame(Context ctx, int *buf_size);
