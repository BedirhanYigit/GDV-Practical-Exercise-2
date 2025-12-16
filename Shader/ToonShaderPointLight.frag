varying vec3 normalES;		// normal in eye space/camera space
varying vec3 positionES;		// vertex position in eye space
varying vec4 baseColor;		// base color

void main()
{
    // Get the light position in eye space. 
    vec3 lightPosES = vec3(gl_LightSource[0].position);

    // Compute light direction vector from the fragment towards the light. 
    // For point light: L = normalize(light position - fragment position). 
    // If directional light: L = normalize(light position). 
    vec3 lightDir = normalize(lightPosES - positionES);

    // Compute intensity using dot product of normal and light direction. 
    float intensity = dot(normalize(normalES), lightDir);

    // Clamp negative values to zero to prevent negative light. 
    intensity = max(intensity, 0.0);

    // Quantize the shading using the computed intensity as threshold. 
    // Distance attenuation is not applied in this shader. 
    float shade;
    if      (intensity > 0.95) shade = 1.0;
    else if (intensity > 0.5)  shade = 0.6;
    else if (intensity > 0.25) shade = 0.4;
    else                       shade = 0.2;

    // Get the final fragment color by multiplying the base color with shading. 
    gl_FragColor = baseColor * shade;
}

