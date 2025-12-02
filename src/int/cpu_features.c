#include "cpu_features.h"
#include "mem/heap.h"
#include "term/klog.h"

cpu_features_t *cpu_features;

void cpu_features_setup() {
    cpu_features = malloc(sizeof(cpu_features_t));
    cpu_features->features = create_string_builder(1024);
    arch_cpuid_feature_info(cpu_features);
    kinfo("cpu features: %s",cpu_features->features->data);
}

cpu_features_t *get_global_features(){
    return cpu_features;
}
