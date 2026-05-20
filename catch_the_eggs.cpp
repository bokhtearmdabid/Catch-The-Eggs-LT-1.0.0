/*
 * CATCH THE EGGS - OpenGL Game
 * Build: g++ catch_the_eggs.cpp -o catch_the_eggs -lGL -lGLU -lglut -lm
 * Or on macOS: g++ catch_the_eggs.cpp -o catch_the_eggs -framework OpenGL -framework GLUT -lm
 */

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>
#include <algorithm>

// ─── Window & World ──────────────────────────────────────────────────────────
const int WIN_W = 800, WIN_H = 600;
const float WORLD_W = 800.f, WORLD_H = 600.f;

// ─── Game constants ───────────────────────────────────────────────────────────
const int   GAME_DURATION     = 60;      // seconds
const float BASKET_SPEED      = 12.f;
const float BASE_EGG_SPEED    = 3.5f;
const float BASE_PERK_SPEED   = 2.5f;
const float STICK_Y           = WORLD_H - 80.f;
const float GROUND_Y          = 40.f;
const float BASKET_W          = 80.f;
const float BASKET_H          = 40.f;
const float EGG_R             = 14.f;
const float PERK_SIZE         = 22.f;
const int   MAX_EGGS          = 12;
const int   MAX_PERKS         = 3;
const float EGG_SPAWN_RATE    = 80.f;   // frames between spawns (base)
const float PERK_SPAWN_RATE   = 300.f;

// ─── Egg types ────────────────────────────────────────────────────────────────
enum EggType { EGG_NORMAL=0, EGG_GOLDEN, EGG_BLUE, EGG_POOP };
// ─── Perk types ───────────────────────────────────────────────────────────────
enum PerkType { PERK_WIDE=0, PERK_SLOW, PERK_TIME, PERK_WIND };

// ─── Game states ─────────────────────────────────────────────────────────────
enum GameState { STATE_MENU=0, STATE_PLAY, STATE_PAUSE, STATE_GAMEOVER, STATE_HELP };

// ─────────────────────────────────────────────────────────────────────────────
struct Egg {
    float x, y;
    float vy;           // falling speed (positive = down)
    EggType type;
    bool active;
    float wobble;       // phase for wobble animation
};

struct Perk {
    float x, y;
    float vy;
    PerkType type;
    bool active;
    float rot;
};

struct Chicken {
    float x;           // centre-x on stick
    float vx;          // auto-move velocity
    float dir;         // 1 or -1
    float stickX1, stickX2; // stick range
    float animPhase;
};

struct Particle {
    float x, y, vx, vy, life, maxLife;
    float r, g, b;
    float size;
};

// ─── Global state ─────────────────────────────────────────────────────────────
GameState   gState          = STATE_MENU;
int         gScore          = 0;
int         gHighScore      = 0;
float       gTimeLeft       = GAME_DURATION;
float       gBasketX        = WORLD_W / 2.f;
float       gBasketW        = BASKET_W;
bool        gKeyLeft        = false, gKeyRight = false;
float       gSpeedMul       = 1.f;       // egg speed multiplier (slow perk)
float       gSlowTimer      = 0.f;
float       gWideTimer      = 0.f;
float       gWindX          = 0.f;       // horizontal drift applied to eggs
float       gWindTimer      = 0.f;
float       gEggSpawnTimer  = 0.f;
float       gPerkSpawnTimer = 0.f;
float       gFrame          = 0.f;
int         gCombo          = 0;
float       gComboTimer     = 0.f;
char        gMessage[64]    = "";
float       gMsgTimer       = 0.f;

// Two sticks / two chickens
Chicken     gChicken[2];
Egg         gEggs[MAX_EGGS];
Perk        gPerks[MAX_PERKS];
std::vector<Particle> gParticles;

// ─── Helpers ──────────────────────────────────────────────────────────────────
float randf(float lo, float hi){ return lo + (hi-lo)*((float)rand()/RAND_MAX); }

void showMessage(const char* msg){
    strncpy(gMessage, msg, 63);
    gMsgTimer = 2.5f;
}

// ─── Drawing helpers ──────────────────────────────────────────────────────────
void setColor(float r,float g,float b,float a=1.f){glColor4f(r,g,b,a);}

void drawRect(float x,float y,float w,float h){
    glBegin(GL_QUADS);
    glVertex2f(x,y); glVertex2f(x+w,y);
    glVertex2f(x+w,y+h); glVertex2f(x,y+h);
    glEnd();
}

void drawCircle(float cx,float cy,float r,int segs=30){
    glBegin(GL_POLYGON);
    for(int i=0;i<segs;i++){
        float a=2.f*M_PI*i/segs;
        glVertex2f(cx+r*cosf(a),cy+r*sinf(a));
    }
    glEnd();
}

void drawEllipse(float cx,float cy,float rx,float ry,int segs=30){
    glBegin(GL_POLYGON);
    for(int i=0;i<segs;i++){
        float a=2.f*M_PI*i/segs;
        glVertex2f(cx+rx*cosf(a),cy+ry*sinf(a));
    }
    glEnd();
}

void drawRing(float cx,float cy,float r,float thickness,int segs=30){
    glLineWidth(thickness);
    glBegin(GL_LINE_LOOP);
    for(int i=0;i<segs;i++){
        float a=2.f*M_PI*i/segs;
        glVertex2f(cx+r*cosf(a),cy+r*sinf(a));
    }
    glEnd();
}

// Draw text using GLUT bitmap fonts
void drawText(float x,float y,const char* txt,void* font=GLUT_BITMAP_HELVETICA_18){
    glRasterPos2f(x,y);
    while(*txt) glutBitmapCharacter(font,(int)*txt++);
}
void drawTextLarge(float x,float y,const char* txt){
    drawText(x,y,txt,GLUT_BITMAP_TIMES_ROMAN_24);
}

// ─── Egg drawing ─────────────────────────────────────────────────────────────
void drawEgg(const Egg& e){
    if(!e.active) return;
    float rx=EGG_R*0.78f, ry=EGG_R;
    float wobble=sinf(e.wobble)*3.f;

    switch(e.type){
    case EGG_NORMAL:
        setColor(1.f,0.97f,0.87f);
        drawEllipse(e.x+wobble,e.y,rx,ry);
        setColor(0.85f,0.80f,0.70f);
        drawRing(e.x+wobble,e.y,rx*0.9f,1.5f);
        break;
    case EGG_GOLDEN:
        // Shiny gold
        setColor(1.f,0.84f,0.f);
        drawEllipse(e.x+wobble,e.y,rx,ry);
        setColor(1.f,0.96f,0.5f);
        drawEllipse(e.x+wobble+rx*0.2f,e.y+ry*0.25f,rx*0.25f,ry*0.18f);
        setColor(0.8f,0.6f,0.f);
        drawRing(e.x+wobble,e.y,rx*0.95f,2.f);
        // sparkle
        setColor(1.f,1.f,0.7f);
        for(int i=0;i<4;i++){
            float a=gFrame*0.05f+i*M_PI/2.f;
            float sx=e.x+cosf(a)*(rx+4.f);
            float sy=e.y+sinf(a)*(ry+4.f);
            drawCircle(sx,sy,2.5f,6);
        }
        break;
    case EGG_BLUE:
        setColor(0.3f,0.6f,1.f);
        drawEllipse(e.x+wobble,e.y,rx,ry);
        setColor(0.6f,0.85f,1.f);
        drawEllipse(e.x+wobble+rx*0.2f,e.y+ry*0.25f,rx*0.2f,ry*0.15f);
        setColor(0.1f,0.3f,0.8f);
        drawRing(e.x+wobble,e.y,rx*0.95f,1.5f);
        break;
    case EGG_POOP:
        // Brown swirl pile
        setColor(0.45f,0.25f,0.05f);
        drawEllipse(e.x,e.y,rx*1.1f,ry*0.7f);
        setColor(0.5f,0.28f,0.07f);
        drawEllipse(e.x,e.y+ry*0.3f,rx*0.8f,ry*0.55f);
        setColor(0.55f,0.32f,0.1f);
        drawEllipse(e.x,e.y+ry*0.7f,rx*0.5f,ry*0.35f);
        // flies
        setColor(0.05f,0.05f,0.05f);
        drawCircle(e.x-rx*0.8f,e.y+ry*0.5f+sinf(gFrame*0.2f)*5.f,2.f,6);
        drawCircle(e.x+rx*0.8f,e.y+ry*0.3f+sinf(gFrame*0.2f+1.f)*5.f,2.f,6);
        break;
    }
}

// ─── Perk drawing ────────────────────────────────────────────────────────────
void drawPerk(const Perk& p){
    if(!p.active) return;
    float s=PERK_SIZE;
    glPushMatrix();
    glTranslatef(p.x,p.y,0);
    glRotatef(p.rot,0,0,1);

    switch(p.type){
    case PERK_WIDE:   // Green arrow expand
        setColor(0.2f,0.9f,0.3f);
        drawRect(-s,-s*0.4f,s*2.f,s*0.8f);
        setColor(0.1f,0.7f,0.15f);
        // arrow tips
        glBegin(GL_TRIANGLES);
        glVertex2f(-s-s*0.5f,0); glVertex2f(-s,s*0.6f); glVertex2f(-s,-s*0.6f);
        glVertex2f(s+s*0.5f,0);  glVertex2f(s,s*0.6f);  glVertex2f(s,-s*0.6f);
        glEnd();
        setColor(1,1,1);
        drawText(-6,-7,"W",GLUT_BITMAP_HELVETICA_12);
        break;
    case PERK_SLOW:   // Blue hourglass
        setColor(0.2f,0.5f,1.f);
        glBegin(GL_TRIANGLES);
        glVertex2f(-s,s); glVertex2f(s,s); glVertex2f(0,0);
        glVertex2f(-s,-s); glVertex2f(s,-s); glVertex2f(0,0);
        glEnd();
        setColor(0.7f,0.9f,1.f);
        drawRect(-s*0.3f,-2,s*0.6f,4);
        setColor(1,1,1);
        drawText(-5,-7,"S",GLUT_BITMAP_HELVETICA_12);
        break;
    case PERK_TIME:   // Yellow clock
        setColor(1.f,0.85f,0.1f);
        drawCircle(0,0,s);
        setColor(0.9f,0.6f,0.f);
        drawRing(0,0,s,2.f);
        setColor(0.3f,0.1f,0.f);
        // clock hands
        glLineWidth(2);
        glBegin(GL_LINES);
        glVertex2f(0,0); glVertex2f(0,s*0.6f);   // 12
        glVertex2f(0,0); glVertex2f(s*0.5f,0);   // 3
        glEnd();
        setColor(1,1,1);
        drawText(-5,-7,"T",GLUT_BITMAP_HELVETICA_12);
        break;
    case PERK_WIND:   // Cyan swirl
        setColor(0.3f,0.95f,0.95f);
        for(int i=0;i<3;i++){
            float a=i*2.f*M_PI/3.f+p.rot*0.05f;
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(0,0);
            for(int j=0;j<=8;j++){
                float b=a+j*0.4f;
                float r2=s*(0.2f+0.8f*(float)j/8.f);
                glVertex2f(cosf(b)*r2,sinf(b)*r2);
            }
            glEnd();
        }
        setColor(1,1,1);
        drawText(-5,-7,"~",GLUT_BITMAP_HELVETICA_12);
        break;
    }
    glPopMatrix();
}


// ─── Chicken drawing ─────────────────────────────────────────────────────────
void drawChicken(const Chicken& c){
    float x=c.x, y=STICK_Y+14.f;
    float bob=sinf(c.animPhase)*3.f;
    y+=bob;

    // Body
    setColor(1.f,0.95f,0.85f);
    drawEllipse(x,y,18,14);
    // Head
    setColor(1.f,0.95f,0.85f);
    drawCircle(x+(c.dir>0?14.f:-14.f),y+8,10);
    // Comb
    setColor(1.f,0.2f,0.2f);
    float hx=x+(c.dir>0?14.f:-14.f);
    drawEllipse(hx,y+18,5,6);
    drawEllipse(hx+c.dir*4.f,y+20,4,5);
    // Beak
    setColor(1.f,0.7f,0.f);
    glBegin(GL_TRIANGLES);
    glVertex2f(hx+c.dir*9.f,y+8);
    glVertex2f(hx+c.dir*14.f,y+10);
    glVertex2f(hx+c.dir*9.f,y+5);
    glEnd();
    // Eye
    setColor(0.05f,0.05f,0.05f);
    drawCircle(hx+c.dir*5.f,y+11,2,8);
    // Wing flap
    float wingY=sinf(c.animPhase*2.f)*6.f;
    setColor(0.95f,0.88f,0.75f);
    glBegin(GL_TRIANGLES);
    glVertex2f(x-c.dir*2.f,y+4);
    glVertex2f(x-c.dir*18.f,y-8+wingY);
    glVertex2f(x-c.dir*8.f,y-8+wingY);
    glEnd();
    // Feet
    setColor(1.f,0.7f,0.f);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(x-6,y-14); glVertex2f(x-6,y-20);
    glVertex2f(x+6,y-14); glVertex2f(x+6,y-20);
    glVertex2f(x-6,y-20); glVertex2f(x-12,y-20);
    glVertex2f(x+6,y-20); glVertex2f(x+12,y-20);
    glEnd();
}

// ─── Basket drawing ──────────────────────────────────────────────────────────
void drawBasket(){
    float bx=gBasketX-gBasketW/2.f;
    float by=GROUND_Y;
    float bw=gBasketW, bh=BASKET_H;

    // Shadow
    setColor(0,0,0,0.18f);
    drawEllipse(gBasketX,by-2,bw*0.55f,6);

    // Weave pattern (brown basket)
    setColor(0.55f,0.27f,0.07f);
    drawRect(bx,by,bw,bh);

    // Weave lines horizontal
    setColor(0.45f,0.20f,0.05f);
    glLineWidth(1.5f);
    for(int i=1;i<5;i++){
        float ly=by+bh*i/5.f;
        glBegin(GL_LINES); glVertex2f(bx,ly); glVertex2f(bx+bw,ly); glEnd();
    }
    // Weave lines vertical
    for(int i=1;i<8;i++){
        float lx=bx+bw*i/8.f;
        glBegin(GL_LINES); glVertex2f(lx,by); glVertex2f(lx,by+bh); glEnd();
    }

    // Rim
    setColor(0.7f,0.4f,0.1f);
    drawRect(bx-4,by+bh-6,bw+8,10);
    // Handle arc
    glLineWidth(3);
    setColor(0.6f,0.3f,0.07f);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<=20;i++){
        float t=(float)i/20.f;
        float hx2=bx+bw*t;
        float hy=by+bh+20.f*sinf(M_PI*t);
        glVertex2f(hx2,hy);
    }
    glEnd();

    // Wide perk glow
    if(gWideTimer>0){
        setColor(0.2f,1.f,0.3f,0.3f);
        drawRect(bx-4,by,bw+8,bh+10);
    }
    // Slow perk glow
    if(gSlowTimer>0){
        setColor(0.3f,0.5f,1.f,0.25f);
        drawRect(bx-4,by,bw+8,bh+10);
    }
}

// ─── Background ──────────────────────────────────────────────────────────────
void drawBackground(){
    // Sky gradient (drawn as quads)
    glBegin(GL_QUADS);
    setColor(0.4f,0.75f,1.f);   glVertex2f(0,WORLD_H); glVertex2f(WORLD_W,WORLD_H);
    setColor(0.7f,0.9f,1.f);    glVertex2f(WORLD_W,GROUND_Y+60); glVertex2f(0,GROUND_Y+60);
    glEnd();

    // Ground
    setColor(0.35f,0.65f,0.25f);
    drawRect(0,0,WORLD_W,GROUND_Y+60);
    setColor(0.45f,0.75f,0.3f);
    drawRect(0,GROUND_Y+40,WORLD_W,20);

    // Clouds
    auto cloud=[](float cx,float cy,float s){
        setColor(1,1,1,0.85f);
        drawCircle(cx,cy,s*0.8f);
        drawCircle(cx+s,cy,s*0.6f);
        drawCircle(cx-s,cy,s*0.55f);
        drawCircle(cx+s*0.4f,cy+s*0.4f,s*0.55f);
    };
    float cf=gFrame*0.15f;
    cloud(fmodf(120+cf,WORLD_W+100)-50, 540,28);
    cloud(fmodf(380+cf*0.7f,WORLD_W+100)-50, 520,22);
    cloud(fmodf(620+cf*1.2f,WORLD_W+100)-50, 550,30);


    // Sticks (bamboo)
    for(int s=0;s<2;s++){
        float sx1=gChicken[s].stickX1-20.f;
        float sx2=gChicken[s].stickX2+20.f;
        // Pole
        setColor(0.5f,0.3f,0.1f);
        drawRect(sx1,GROUND_Y+55,6,STICK_Y-GROUND_Y-55);
        drawRect(sx2-6,GROUND_Y+55,6,STICK_Y-GROUND_Y-55);
        // Bamboo stick
        setColor(0.68f,0.85f,0.35f);
        drawRect(sx1,STICK_Y-6,sx2-sx1,12);
        // Bamboo nodes
        setColor(0.5f,0.7f,0.2f);
        for(float nx=sx1+40;nx<sx2-20;nx+=40){
            drawRect(nx-3,STICK_Y-8,6,16);
        }
    }

    // Wind indicator
    if(gWindTimer>0){
        float alpha=std::min(1.f, gWindTimer/3.f);
        setColor(0.3f,0.9f,0.9f,alpha);
        float wx=gWindX>0?WORLD_W-80:80;
        drawText(wx,WORLD_H/2.f+50, gWindX>0?">>> WIND >>>" :"<<< WIND <<<");
        // arrows
        glLineWidth(2);
        for(int i=0;i<3;i++){
            float ay=WORLD_H/2.f-30+i*30;
            float ax=gWindX>0? 30.f+gFrame*0.5f : WORLD_W-30.f-gFrame*0.5f;
            ax=fmodf(ax, WORLD_W);
            glBegin(GL_LINE_STRIP);
            glVertex2f(ax,ay);
            glVertex2f(ax+gWindX*0.4f,ay);
            glEnd();
        }
    }
}

// ─── HUD ─────────────────────────────────────────────────────────────────────
void drawHUD(){
    // Score panel
    setColor(0,0,0,0.4f);
    drawRect(5,WIN_H-55,200,50);
    setColor(1.f,0.95f,0.2f);
    char buf[64];
    sprintf(buf,"Score: %d",gScore);
    drawTextLarge(12,WIN_H-30,buf);
    setColor(0.8f,0.8f,0.8f);
    sprintf(buf,"Best: %d",gHighScore);
    drawText(12,WIN_H-48,buf,GLUT_BITMAP_HELVETICA_12);

    // Timer
    setColor(0,0,0,0.4f);
    drawRect(WIN_W-120,WIN_H-55,115,50);
    float tl=gTimeLeft;
    if(tl<10) setColor(1.f,0.3f,0.3f);
    else setColor(0.3f,1.f,0.5f);
    sprintf(buf,"Time: %.1f",tl);
    drawTextLarge(WIN_W-110,WIN_H-30,buf);

    // Combo
    if(gCombo>=3 && gComboTimer>0){
        float cf=fminf(1.f,gComboTimer);
        setColor(1.f,0.5f,0.f,cf);
        sprintf(buf,"COMBO x%d!",gCombo);
        drawTextLarge(WIN_W/2.f-60,WIN_H-70,buf);
    }

    // Active perks
    float px=5;
    if(gWideTimer>0){
        setColor(0.2f,0.9f,0.3f,0.85f);
        sprintf(buf,"[WIDE %.1fs]",gWideTimer); drawText(px,50,buf); px+=130;
    }
    if(gSlowTimer>0){
        setColor(0.3f,0.6f,1.f,0.85f);
        sprintf(buf,"[SLOW %.1fs]",gSlowTimer); drawText(px,50,buf); px+=130;
    }
    if(gWindTimer>0){
        setColor(0.3f,0.95f,0.95f,0.85f);
        sprintf(buf,"[WIND %.1fs]",gWindTimer); drawText(px,50,buf);
    }

    // Floating message
    if(gMsgTimer>0){
        float alpha=fminf(1.f,gMsgTimer);
        setColor(1.f,1.f,0.3f,alpha);
        drawTextLarge(WIN_W/2.f-80, WIN_H/2.f, gMessage);
    }
}

// ─── Particle system ─────────────────────────────────────────────────────────
void spawnParticles(float x,float y,int n,float r,float g2,float b2){
    for(int i=0;i<n;i++){
        Particle p;
        p.x=x; p.y=y;
        float a=randf(0,2*M_PI), spd=randf(1.5f,5.f);
        p.vx=cosf(a)*spd; p.vy=sinf(a)*spd;
        p.life=p.maxLife=randf(0.5f,1.2f);
        p.r=r; p.g=g2; p.b=b2;
        p.size=randf(3,8);
        gParticles.push_back(p);
    }
}

void updateParticles(float dt){
    for(auto& p:gParticles){
        p.x+=p.vx; p.y+=p.vy;
        p.vy-=0.12f;
        p.life-=dt;
    }
    gParticles.erase(std::remove_if(gParticles.begin(),gParticles.end(),
        [](const Particle& p){return p.life<=0;}),gParticles.end());
}

void drawParticles(){
    for(auto& p:gParticles){
        float a=p.life/p.maxLife;
        setColor(p.r,p.g,p.b,a);
        drawCircle(p.x,p.y,p.size*a,8);
    }
}

// ─── Spawn helpers ────────────────────────────────────────────────────────────
void spawnEgg(){
    // pick which stick to spawn from
    int si=rand()%2;
    Chicken& c=gChicken[si];
    for(int i=0;i<MAX_EGGS;i++){
        if(!gEggs[i].active){
            gEggs[i].active=true;
            gEggs[i].x=c.x;
            gEggs[i].y=STICK_Y-12.f;
            float r=randf(0,1);
            if(r<0.08f) gEggs[i].type=EGG_GOLDEN;
            else if(r<0.25f) gEggs[i].type=EGG_BLUE;
            else if(r<0.35f) gEggs[i].type=EGG_POOP;
            else gEggs[i].type=EGG_NORMAL;
            gEggs[i].vy=BASE_EGG_SPEED+randf(0,2.f);
            gEggs[i].wobble=randf(0,2*M_PI);
            break;
        }
    }
}

void spawnPerk(){
    for(int i=0;i<MAX_PERKS;i++){
        if(!gPerks[i].active){
            gPerks[i].active=true;
            gPerks[i].x=randf(40,WORLD_W-40);
            gPerks[i].y=WORLD_H+10;
            gPerks[i].vy=BASE_PERK_SPEED+randf(0,1.f);
            gPerks[i].type=(PerkType)(rand()%4);
            gPerks[i].rot=randf(0,360);
            break;
        }
    }
}

// ─── Collision helpers ────────────────────────────────────────────────────────
bool inBasket(float ex,float ey){
    float bx=gBasketX-gBasketW/2.f;
    return ex>=bx-EGG_R && ex<=bx+gBasketW+EGG_R &&
           ey<=GROUND_Y+BASKET_H && ey>=GROUND_Y-EGG_R;
}

bool inBasketPerk(float px2,float py){
    float bx=gBasketX-gBasketW/2.f;
    return px2>=bx && px2<=bx+gBasketW &&
           py<=GROUND_Y+BASKET_H && py>=GROUND_Y-PERK_SIZE;
}

// ─── Init ─────────────────────────────────────────────────────────────────────
void initGame(){
    gScore=0; gTimeLeft=GAME_DURATION;
    gBasketX=WORLD_W/2.f; gBasketW=BASKET_W;
    gSpeedMul=1.f; gSlowTimer=0; gWideTimer=0; gWindX=0; gWindTimer=0;
    gEggSpawnTimer=0; gPerkSpawnTimer=0; gFrame=0;
    gCombo=0; gComboTimer=0; gMsgTimer=0;

    for(int i=0;i<MAX_EGGS;i++) gEggs[i].active=false;
    for(int i=0;i<MAX_PERKS;i++) gPerks[i].active=false;
    gParticles.clear();

    // Chicken 0: left stick
    gChicken[0]={200,2,1, 80,380, 0};
    // Chicken 1: right stick
    gChicken[1]={550,1.5f,1, 420,720, 0};
}

// ─── Update ───────────────────────────────────────────────────────────────────
void update(float dt){
    gFrame+=1;

    // Timers
    if(gTimeLeft<=0){ gTimeLeft=0; gHighScore=std::max(gHighScore,gScore); gState=STATE_GAMEOVER; return; }
    gTimeLeft-=dt;

    // Perks timer
    if(gSlowTimer>0){ gSlowTimer-=dt; if(gSlowTimer<=0){gSpeedMul=1.f; showMessage("Speed restored!");} }
    if(gWideTimer>0){ gWideTimer-=dt; if(gWideTimer<=0){gBasketW=BASKET_W; showMessage("Basket normal.");} }
    if(gWindTimer>0){ gWindTimer-=dt; if(gWindTimer<=0){gWindX=0;} }
    if(gComboTimer>0) gComboTimer-=dt;
    if(gMsgTimer>0) gMsgTimer-=dt;

    // Basket movement
    float bspd = BASKET_SPEED + gBasketW*0.05f;
    if(gKeyLeft)  gBasketX-=bspd;
    if(gKeyRight) gBasketX+=bspd;
    gBasketX=std::max(gBasketW/2.f+4, std::min(WORLD_W-gBasketW/2.f-4, gBasketX));

    // Chickens
    for(int s=0;s<2;s++){
        Chicken& c=gChicken[s];
        c.x+=c.vx*c.dir;
        if(c.x>c.stickX2){c.dir=-1;}
        if(c.x<c.stickX1){c.dir=1;}
        c.animPhase+=0.12f;
    }

    // Egg spawn
    gEggSpawnTimer+=1;
    float spawnRate=EGG_SPAWN_RATE/(1.f+gFrame/3000.f); // gets faster over time
    if(gEggSpawnTimer>=spawnRate){ gEggSpawnTimer=0; spawnEgg(); }

    // Perk spawn
    gPerkSpawnTimer+=1;
    if(gPerkSpawnTimer>=PERK_SPAWN_RATE){ gPerkSpawnTimer=0; spawnPerk(); }

    // Update eggs
    for(int i=0;i<MAX_EGGS;i++){
        Egg& e=gEggs[i];
        if(!e.active) continue;
        e.y-=e.vy*gSpeedMul;
        e.x+=gWindX*dt*20.f;
        e.wobble+=0.1f;

        if(e.y<=GROUND_Y+BASKET_H && e.y>=GROUND_Y-EGG_R){
            if(inBasket(e.x,e.y)){
                int pts=0;
                float pr=1,pg=1,pb=1;
                switch(e.type){
                case EGG_NORMAL: pts=1; pr=1;pg=0.97f;pb=0.87f; break;
                case EGG_GOLDEN: pts=10; pr=1;pg=0.84f;pb=0.f; break;
                case EGG_BLUE:   pts=5; pr=0.3f;pg=0.6f;pb=1.f; break;
                case EGG_POOP:   pts=-10; pr=0.45f;pg=0.25f;pb=0.05f;
                    gCombo=0; showMessage("-10! Don't catch poop!"); break;
                }
                if(e.type!=EGG_POOP){
                    gCombo++;
                    gComboTimer=2.f;
                    if(gCombo>=5) pts*=2;
                    if(e.type==EGG_GOLDEN) showMessage("+10! Golden Egg!");
                    else if(e.type==EGG_BLUE) showMessage("+5! Blue Egg!");
                }
                gScore=std::max(0,gScore+pts);
                spawnParticles(e.x,e.y,12,pr,pg,pb);
                e.active=false;
                continue;
            }
        }

        // Hit ground uncaught
        if(e.y<GROUND_Y-EGG_R){
            e.active=false;
            if(e.type!=EGG_POOP) gCombo=0; // miss breaks combo
        }
        // Went off screen sides
        if(e.x<-20||e.x>WORLD_W+20) e.active=false;
    }

    // Update perks
    for(int i=0;i<MAX_PERKS;i++){
        Perk& p=gPerks[i];
        if(!p.active) continue;
        p.y-=p.vy;
        p.rot+=2.f;
        if(inBasketPerk(p.x,p.y)){
            // Apply perk
            switch(p.type){
            case PERK_WIDE:
                gBasketW=std::min(BASKET_W*2.2f,gBasketW+40.f);
                gWideTimer=8.f;
                showMessage("Wider basket! (+8s)");
                spawnParticles(p.x,p.y,15,0.2f,0.9f,0.3f);
                break;
            case PERK_SLOW:
                gSpeedMul=0.45f;
                gSlowTimer=7.f;
                showMessage("Eggs slowed! (+7s)");
                spawnParticles(p.x,p.y,15,0.3f,0.6f,1.f);
                break;
            case PERK_TIME:
                gTimeLeft=std::min((float)GAME_DURATION,gTimeLeft+12.f);
                showMessage("+12 seconds!");
                spawnParticles(p.x,p.y,15,1.f,0.85f,0.1f);
                break;
            case PERK_WIND:
                gWindX=randf(-1.f,1.f)>0?1.f:-1.f;
                gWindTimer=5.f;
                showMessage("Wind changes direction!");
                spawnParticles(p.x,p.y,15,0.3f,0.95f,0.95f);
                break;
            }
            p.active=false;
            continue;
        }
        if(p.y<GROUND_Y-PERK_SIZE) p.active=false;
    }

    updateParticles(dt);
}

// ─── Menus ───────────────────────────────────────────────────────────────────
void drawMenuBG(){
    glBegin(GL_QUADS);
    setColor(0.1f,0.05f,0.2f); glVertex2f(0,0); glVertex2f(WORLD_W,0);
    setColor(0.2f,0.1f,0.4f);  glVertex2f(WORLD_W,WORLD_H); glVertex2f(0,WORLD_H);
    glEnd();
    // Stars
    srand(42);
    setColor(1,1,1,0.7f);
    for(int i=0;i<80;i++){
        float sx=randf(0,WORLD_W), sy=randf(0,WORLD_H);
        float sz=randf(1,3);
        float twinkle=0.5f+0.5f*sinf(gFrame*0.04f+i);
        setColor(1,1,1,twinkle);
        drawCircle(sx,sy,sz,6);
    }
    srand((unsigned)time(nullptr));
}

void drawMainMenu(){
    drawMenuBG();

    // Title shadow
    setColor(0.5f,0.0f,0.5f,0.6f);
    drawTextLarge(WIN_W/2.f-148, WIN_H-152, "CATCH THE EGGS");
    // Title
    setColor(1.f,0.9f,0.1f);
    drawTextLarge(WIN_W/2.f-150, WIN_H-148, "CATCH THE EGGS");

    // Animated egg samples
    float t=gFrame*0.05f;
    float ey=WIN_H/2.f+80+sinf(t)*20;
    setColor(1.f,0.97f,0.87f); drawEllipse(WIN_W/2.f-120, ey, 12,16);
    setColor(1.f,0.84f,0.f);   drawEllipse(WIN_W/2.f,     ey, 12,16);
    setColor(0.3f,0.6f,1.f);   drawEllipse(WIN_W/2.f+120, ey, 12,16);

    setColor(0.8f,0.8f,0.8f);
    drawTextLarge(WIN_W/2.f-110, WIN_H/2.f+20, "Press  ENTER  to Start");
    drawText(WIN_W/2.f-100, WIN_H/2.f-10, "Press  H  for Help",GLUT_BITMAP_HELVETICA_18);
    drawText(WIN_W/2.f-100, WIN_H/2.f-40, "Press  ESC  to Exit",GLUT_BITMAP_HELVETICA_18);

    // High score
    if(gHighScore>0){
        char buf[64]; sprintf(buf,"High Score: %d",gHighScore);
        setColor(1.f,0.7f,0.1f);
        drawTextLarge(WIN_W/2.f-90, 80, buf);
    }

    // Egg legend
    setColor(0.7f,0.7f,0.7f);
    drawText(20, 110, "Golden=10pts  Blue=5pts  White=1pt  Poop=-10pts", GLUT_BITMAP_HELVETICA_12);
}

void drawPauseMenu(){
    // Dim
    setColor(0,0,0,0.55f);
    drawRect(0,0,WORLD_W,WORLD_H);

    setColor(0.1f,0.1f,0.3f,0.9f);
    drawRect(WIN_W/2.f-140,WIN_H/2.f-100,280,220);

    setColor(1.f,0.9f,0.2f);
    drawTextLarge(WIN_W/2.f-70, WIN_H/2.f+100, "PAUSED");
    setColor(0.9f,0.9f,0.9f);
    drawText(WIN_W/2.f-110, WIN_H/2.f+55, "Press P to Resume", GLUT_BITMAP_HELVETICA_18);
    drawText(WIN_W/2.f-110, WIN_H/2.f+20, "Press H for Help",  GLUT_BITMAP_HELVETICA_18);
    drawText(WIN_W/2.f-110, WIN_H/2.f-15, "Press M for Menu",  GLUT_BITMAP_HELVETICA_18);
    drawText(WIN_W/2.f-110, WIN_H/2.f-50, "Press ESC to Quit", GLUT_BITMAP_HELVETICA_18);
}

void drawGameOver(){
    drawMenuBG();
    setColor(1.f,0.2f,0.2f);
    drawTextLarge(WIN_W/2.f-100, WIN_H-150, "GAME  OVER");
    char buf[64];
    sprintf(buf,"Your Score: %d",gScore);
    setColor(1.f,0.9f,0.1f);
    drawTextLarge(WIN_W/2.f-100, WIN_H/2.f+60, buf);
    sprintf(buf,"High Score: %d",gHighScore);
    setColor(0.9f,0.7f,0.3f);
    drawTextLarge(WIN_W/2.f-100, WIN_H/2.f+20, buf);
    setColor(0.8f,0.8f,0.8f);
    drawText(WIN_W/2.f-110, WIN_H/2.f-30, "ENTER - Play Again", GLUT_BITMAP_HELVETICA_18);
    drawText(WIN_W/2.f-110, WIN_H/2.f-60, "M     - Main Menu",  GLUT_BITMAP_HELVETICA_18);
    drawText(WIN_W/2.f-110, WIN_H/2.f-90, "ESC   - Quit",       GLUT_BITMAP_HELVETICA_18);
}

void drawHelp(){
    drawMenuBG();
    setColor(1.f,0.9f,0.1f);
    drawTextLarge(WIN_W/2.f-70, WIN_H-70, "HELP / CONTROLS");
    setColor(0.9f,0.9f,0.9f);
    const char* lines[]={
        "LEFT / RIGHT Arrow  - Move basket",
        "Mouse move          - Move basket",
        "P                   - Pause / Resume",
        "M                   - Main Menu",
        "ESC                 - Quit",
        "",
        "EGG TYPES:",
        "White egg  = +1 point",
        "Blue egg   = +5 points",
        "Golden egg = +10 points",
        "Poop       = -10 points  (AVOID!)",
        "",
        "POWER-UPS (falling blocks):",
        "GREEN  = Wider basket for 8s",
        "BLUE   = Slow eggs for 7s",
        "YELLOW = +12 extra seconds",
        "CYAN   = Wind drift!",
        "",
        "Combo 5+ catches: score doubled!"
    };
    int n=sizeof(lines)/sizeof(lines[0]);
    for(int i=0;i<n;i++){
        if(lines[i][0]=='\0') continue;
        setColor(0.85f,0.85f,1.f);
        drawText(80, WIN_H-110-i*22, lines[i], GLUT_BITMAP_HELVETICA_12);
    }
    setColor(1.f,0.6f,0.1f);
    drawText(WIN_W/2.f-100, 30, "Press BACKSPACE to go back", GLUT_BITMAP_HELVETICA_18);
}

// ─── Display ─────────────────────────────────────────────────────────────────
void display(){
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    if(gState==STATE_MENU){ drawMainMenu(); glutSwapBuffers(); return; }
    if(gState==STATE_GAMEOVER){ drawGameOver(); glutSwapBuffers(); return; }
    if(gState==STATE_HELP){ drawHelp(); glutSwapBuffers(); return; }

    // ─── Gameplay scene ──────────────────────────
    drawBackground();

    for(int i=0;i<2;i++) drawChicken(gChicken[i]);
    for(int i=0;i<MAX_PERKS;i++) drawPerk(gPerks[i]);
    for(int i=0;i<MAX_EGGS;i++) drawEgg(gEggs[i]);
    drawParticles();
    drawBasket();
    drawHUD();

    if(gState==STATE_PAUSE) drawPauseMenu();

    glutSwapBuffers();
}

// ─── Timer callback ──────────────────────────────────────────────────────────
void timer(int){
    if(gState==STATE_PLAY){
        update(1.f/60.f);
    }
    glutPostRedisplay();
    glutTimerFunc(1000/60, timer, 0);
}

// ─── Keyboard ────────────────────────────────────────────────────────────────
void keyDown(unsigned char key, int, int){
    switch(key){
    case 13: // Enter
        if(gState==STATE_MENU||gState==STATE_GAMEOVER){ initGame(); gState=STATE_PLAY; }
        break;
    case 'p': case 'P':
        if(gState==STATE_PLAY) gState=STATE_PAUSE;
        else if(gState==STATE_PAUSE) gState=STATE_PLAY;
        break;
    case 'm': case 'M':
        gHighScore=std::max(gHighScore,gScore);
        gState=STATE_MENU;
        break;
    case 'h': case 'H':
        gState=STATE_HELP;
        break;
    case 8: // Backspace
        if(gState==STATE_HELP) gState=(gScore>0&&gTimeLeft>0)?STATE_PAUSE:STATE_MENU;
        break;
    case 27: // ESC
        if(gState==STATE_PLAY||gState==STATE_PAUSE) gState=STATE_PAUSE;
        else exit(0);
        break;
    }
}

void specialDown(int key,int,int){
    if(key==GLUT_KEY_LEFT)  gKeyLeft=true;
    if(key==GLUT_KEY_RIGHT) gKeyRight=true;
}
void specialUp(int key,int,int){
    if(key==GLUT_KEY_LEFT)  gKeyLeft=false;
    if(key==GLUT_KEY_RIGHT) gKeyRight=false;
}

// ─── Mouse ───────────────────────────────────────────────────────────────────
void mouseMove(int mx, int){
    if(gState==STATE_PLAY){
        gBasketX=(float)mx;
        gBasketX=std::max(gBasketW/2.f+4, std::min(WORLD_W-gBasketW/2.f-4, gBasketX));
    }
}

// ─── Reshape ─────────────────────────────────────────────────────────────────
void reshape(int w, int h){
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0,WORLD_W,0,WORLD_H);
    glMatrixMode(GL_MODELVIEW);
}

// ─── Main ────────────────────────────────────────────────────────────────────
int main(int argc, char** argv){
    srand((unsigned)time(nullptr));
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);
    glutInitWindowSize(WIN_W,WIN_H);
    glutCreateWindow("Catch the Eggs!");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f,0.1f,0.2f,1);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyDown);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutPassiveMotionFunc(mouseMove);
    glutMotionFunc(mouseMove);
    glutTimerFunc(0, timer, 0);

    initGame();
    gState=STATE_MENU;

    glutMainLoop();
    return 0;
}
