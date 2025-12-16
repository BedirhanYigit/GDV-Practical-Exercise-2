varying vec3 normalES;		// normal in eye space/camera space
varying vec3 positionES;		// vertex position in eye space
varying vec4 baseColor;		// base color

void main()
{
    // Transform vertex position from model space to eye space. 
    positionES = vec3(gl_ModelViewMatrix * gl_Vertex);

    // Transform vertex normal into eye space. 
    normalES = normalize(gl_NormalMatrix * gl_Normal);

    // Pass the current vertex color to fragment shader. 
    baseColor = gl_Color;

    // Standard fixed-function compatible position transform. 
    gl_Position = ftransform();
}

