uniform sampler2D gTextureSampler;
uniform vec4 gGrade; // exposure stops, contrast, saturation, gamma
uniform vec4 gDepthBloom; // highlights, shadows, bloom strength, threshold
uniform vec4 gSpreadVignette; // radius pixels, vignette, unused, unused
uniform vec4 gTint; // RGB, strength including alpha
noperspective in vec2 texCoord;
out vec4 FragClr;
vec3 bright(vec2 uv)
{
 vec3 c = texture(gTextureSampler, uv).rgb;
 float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
 return c * max(l - gDepthBloom.w, 0.0) / max(l, 0.0001);
}
void main()
{
 vec4 source = texture(gTextureSampler, texCoord);
 vec3 c = source.rgb;
 if(gDepthBloom.z > 0.0)
 {
  vec2 d = gSpreadVignette.x / vec2(textureSize(gTextureSampler, 0));
  vec3 b = bright(texCoord) * 4.0;
  b += (bright(texCoord + vec2(d.x, 0.0)) + bright(texCoord - vec2(d.x, 0.0)) +
        bright(texCoord + vec2(0.0, d.y)) + bright(texCoord - vec2(0.0, d.y))) * 2.0;
  b += bright(texCoord + d) + bright(texCoord - d) +
       bright(texCoord + vec2(d.x, -d.y)) + bright(texCoord + vec2(-d.x, d.y));
  c += b * (gDepthBloom.z / 16.0);
 }
 c *= exp2(gGrade.x);
 float l = clamp(dot(c, vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0);
 c += gDepthBloom.y * (1.0 - l) * (1.0 - l) * 0.5;
 c += gDepthBloom.x * l * l * 0.5;
 c = (c - 0.5) * gGrade.y + 0.5;
 c = mix(vec3(dot(c, vec3(0.2126, 0.7152, 0.0722))), c, gGrade.z);
 c = pow(clamp(c, 0.0, 1.0), vec3(1.0 / gGrade.w));
 c = mix(c, c * gTint.rgb, gTint.a);
 vec2 p = texCoord * 2.0 - 1.0;
 c *= 1.0 - gSpreadVignette.y * smoothstep(0.25, 1.5, dot(p, p));
 FragClr = vec4(clamp(c, 0.0, 1.0), source.a);
}
