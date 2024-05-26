#include "run.hh"

#include <barrier>
#include <cassert>
#include <cstring>

#include "log.hh"

void run(nuv_process *ctx, std::vector<std::string> argv, std::vector<char *> env, run_callback callback) {
  assert(ctx->length == 0);

  uv_pipe_init(uv_default_loop(), &ctx->pipe, false);
  ctx->process.data = ctx;
  ctx->pipe.data = ctx;
  ctx->callback = callback;
  ctx->length = 0;

  uv_process_options_t options{};
  uv_stdio_container_t child_stdio[2];
  child_stdio[0].flags = UV_IGNORE;
  child_stdio[1].flags = (uv_stdio_flags)(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
  child_stdio[1].data.stream = (uv_stream_t *)&ctx->pipe;
  options.stdio = child_stdio;
  options.stdio_count = 2;
  std::vector<char *> cargv;
  for (auto &arg : argv)
    cargv.push_back(arg.data());
  cargv.push_back(NULL);
  options.args = cargv.data();
  options.file = cargv[0];
  options.exit_cb = [](uv_process_t *process, int64_t status, int signal) {
    auto *ctx = (nuv_process *)process->data;

    uv_close((uv_handle_t *)process,
             [](uv_handle_t *handle) { (void)((nuv_process *)handle->data)->close_synchronizer.arrive(); });
    uv_close((uv_handle_t *)&ctx->pipe,
             [](uv_handle_t *handle) { (void)((nuv_process*)handle->data)->close_synchronizer.arrive(); });

    ctx->status = status;
    ctx->signal = signal;
    ctx->buffer.resize(ctx->length);
    ctx->length = 0;

    (void)ctx->close_synchronizer.arrive();
  };

  options.env = env.data();

  if (int err = uv_spawn(uv_default_loop(), &ctx->process, &options); err < 0)
    throw std::runtime_error("libuv spawn failed: " + std::string(uv_strerror(err)));

  uv_read_start((uv_stream_t *)&ctx->pipe,
                [](uv_handle_t *handle, size_t size, uv_buf_t *buf) {
                  auto *self = (nuv_process *)handle->data;
                  self->buffer.resize(self->length + size);
                  buf->base = self->buffer.data() + self->length;
                  buf->len = size;
                },
                [](uv_stream_t *stream, ssize_t nread, uv_buf_t const *) {
                  auto *self = (nuv_process *)stream->data;

                  if (nread == UV_EOF)
                    return;
                  else if (nread < 0)
                    error << "Reading child pipe failed!\n";
                  else {
                    self->length += nread;
                  }
                });
}
