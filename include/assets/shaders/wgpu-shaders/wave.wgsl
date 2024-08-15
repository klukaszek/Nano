struct Uniforms {
    @align(8) time: f32,
    @align(8) resolution: vec2<f32>,
    @align(4) frequency: f32,
    @align(4) amplitude: f32,
    @align(4) speed: f32,
    @align(4) thickness: f32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var pos = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(3.0, -1.0),
        vec2<f32>(-1.0, 3.0)
    );
    var uv = array<vec2<f32>, 3>(
        vec2<f32>(0.0, 1.0),
        vec2<f32>(2.0, 1.0),
        vec2<f32>(0.0, -1.0)
    );
    var output: VertexOutput;
    output.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
    output.uv = uv[vertexIndex];
    return output;
}

@fragment
fn fs_main(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {

    // Parameters for the sine wave
    let time = uniforms.time;
    let frequency = uniforms.frequency;
    let amplitude = uniforms.amplitude;
    let speed = uniforms.speed;
    let thickness = uniforms.thickness;

    // Calculate the y-position of the sine wave
    let wave_y = sin((uv.x + time * speed) * frequency) * amplitude + 0.5;

    // Calculate distance from current pixel to the sine wave
    let dist = abs(uv.y - wave_y);

    // Smoothstep for anti-aliasing
    let line = 1.0 - smoothstep(0.0, thickness, dist);

    // Wave color
    let waveColor = vec3<f32>(0.0, 0.7, 1.0);  // Light blue color

    // Background color
    let bgColor = vec3<f32>(0.1, 0.1, 0.1);

    let finalColor = mix(bgColor, waveColor, line);

    return vec4<f32>(finalColor, 1.0);
}
