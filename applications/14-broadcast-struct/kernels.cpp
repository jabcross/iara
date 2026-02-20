#include <IaraRuntime/common/Scheduler.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>

// TestStruct layout: i32(4) + i32(4) + f64(8) + i32(4) + pad(4) = 24 bytes.
// 24 bytes > 8 (pointer size), so the broadcast bug produces num_copies = 8/24 = 0.
struct TestStruct {
  int a;
  int b;
  double c;
  int d;
};

static std::atomic<int> success_count{0};

extern "C" {

void producer(TestStruct data[1]) {
  data[0].a = 1;
  data[0].b = 2;
  data[0].c = 3.0;
  data[0].d = 4;
}

void consumer_a(TestStruct data[1]) {
  if (data[0].a == 1 && data[0].b == 2 && data[0].c == 3.0 && data[0].d == 4) {
    printf("consumer_a ok\n");
    success_count++;
  } else {
    fprintf(stderr,
            "ERROR: consumer_a got a=%d b=%d c=%f d=%d (expected 1 2 3.0 4)\n",
            data[0].a, data[0].b, data[0].c, data[0].d);
  }
}

void consumer_b(TestStruct data[1]) {
  if (data[0].a == 1 && data[0].b == 2 && data[0].c == 3.0 && data[0].d == 4) {
    printf("consumer_b ok\n");
    success_count++;
  } else {
    fprintf(stderr,
            "ERROR: consumer_b got a=%d b=%d c=%f d=%d (expected 1 2 3.0 4)\n",
            data[0].a, data[0].b, data[0].c, data[0].d);
  }
}

void exec() {
  iara_runtime_init();
  iara_runtime_run_iteration(0, 1);

  if (success_count != 2) {
    fprintf(stderr,
            "ERROR: Expected 2 successful consumers, got %d\n",
            success_count.load());
    exit(1);
  }
  printf("Test passed: Both consumers received broadcast struct correctly\n");
}

} // extern "C"

int main() {
  iara_runtime_exec(exec);
  return 0;
}
