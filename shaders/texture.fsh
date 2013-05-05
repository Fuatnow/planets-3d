uniform sampler2D texture;
varying highp vec2 texCoord;

void main(void){
    gl_FragColor = texture2D(texture, texCoord);
}
