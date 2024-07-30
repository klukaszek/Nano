struct Data {
    @location(0) value: f32,
};

@group(0) @binding(0) var<storage, read_write> input : array<Data>;
@group(0) @binding(1) var<storage, read_write> output : array<Data>;

fn f(x: f32) -> f32 {
    return x + 1.0;
}

@compute @workgroup_size(32)
fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let index = global_id.x;

    let max_iterations = 100000;

    // Run the kernel 100000 times to test the performance of Nano's
    // compute shader compiler.
    for (var i = 0; i < max_iterations; i = i + 1) {
        output[index].value = f(input[index].value);
        input[index].value = output[index].value;
    }
}
