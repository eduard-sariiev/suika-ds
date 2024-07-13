// SPDX-License-Identifier: CC0-1.0
//
// SPDX-FileContributor: NightFox & Co., 2009-2011
//
// NightFox's Lib Template
// http://www.nightfoxandco.com

#include <stdio.h>
#include <time.h>
#include <math.h>

#include <nds/arm9/linkedlist.h>

#include <nds.h>
#include <filesystem.h>

#include <nf_lib.h>
#include <chipmunk.h>

#define MASS_MULTIPLIER 1
#define TOTAL_FRUITS 9
#define DOUBLE_SCREEN_HEIGHT 384
#define ALLOW_DROP 60
#define BALL_FRICTION 0.7f
#define TIME_STEP 20.0f
#define SLEEP_TIME_THRESHOLD 1.f

#define doOverlap(x1,  y1,  r1,  x2,  y2,  r2) (fabs((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2)) <= (r1 + r2) * (r1 + r2))
#define getSpriteSize(x) ((x <= 16) ? 16 : (x <= 32) ? 32 : 64)

const s8 sizes[TOTAL_FRUITS] = {16, 22, 28, 34, 40, 46, 52, 58, 64};

const cpCollisionType BALL_COLLISION = 1;
bool taken_sprites[SPRITE_COUNT];
LinkedList *head = NULL;

typedef struct
{
    s8 gfx_id;
    cpVect pos;
} CallbackProp;


typedef struct FruitProperties 
{
    s8 id, gfx_id, diameter, radius, sprite_size_half;
    bool bottom, hideFromTop;
    cpBody *body;
} FruitProperties;

static cpShape* create_ball(s8 id, s8 gfx_id, float x, float y)
{
    cpShape *shape = malloc(sizeof shape); // check if malloc successful
    s8 diameter = sizes[gfx_id], radius = diameter/2;
    float mass = radius * MASS_MULTIPLIER;

    FruitProperties *prop = malloc(sizeof *prop);
    *prop = (FruitProperties) {
        .id = id, .gfx_id = gfx_id, .diameter = diameter, .radius = radius, 
        .sprite_size_half = getSpriteSize(diameter)/2, .bottom = false, .hideFromTop = false, 
        .body = cpBodyNew(mass, cpMomentForCircle(mass, 0, radius, cpvzero))
        };
    
    shape = cpCircleShapeNew(prop->body, radius, cpvzero);
    cpShapeSetFriction(shape, BALL_FRICTION);
    cpShapeSetUserData(shape, prop);
    cpShapeSetCollisionType(shape, BALL_COLLISION);
    cpBodySetPosition(prop->body, cpv(x, y));
    NF_CreateSprite(0, id, gfx_id, gfx_id, -256, -256); // render first time off-screen since coordinates passed here are global
    return shape;
}


static void LoadAssets(void)
{
    // Loading sprites from nitrofs
    for (s8 i = 0; i < TOTAL_FRUITS; i++)
    {
        char temp[17];
        s8 ball_size = sizes[i];
        sprintf(temp, "sprite/ball_%d", ball_size);

        // Load sprite files from NitroFS based on size
        if (ball_size <= 16)
            NF_LoadSpriteGfx(temp, i, 16, 16);
        else if (ball_size <= 32)
            NF_LoadSpriteGfx(temp, i, 32, 32);
        else
            NF_LoadSpriteGfx(temp, i, 64, 64);
        
        // Load palette for sprite
        NF_LoadSpritePal(temp, i);

        // Transfer the required sprites to VRAM
        NF_VramSpriteGfx(0, i, i, false);
        NF_VramSpriteGfx(1, i, i, false);
        NF_VramSpritePal(0, i, i);
        NF_VramSpritePal(1, i, i);
    }

    // Loading and displaying backgrounds
    NF_LoadTiledBg("bg/wall", "wall", 256, 256);
    NF_CreateTiledBg(0, 3, "wall");

    NF_LoadTiledBg("bg/bottom_bg", "bg", 256, 256);
    NF_CreateTiledBg(1, 3, "bg");
}

static s8 getSpriteId(void)
{
    s8 free_id;
    for (free_id = 0; free_id < SPRITE_COUNT; free_id++)
    {
        if (taken_sprites[free_id])
        {
            taken_sprites[free_id] = 0;
            break;
        }
    }
    return free_id;
}

static void removeFruit(cpSpace *space, cpShape *shape, void *unused)
{
    cpSpaceRemoveShape(space, shape);
    cpSpaceRemoveBody(space, cpShapeGetBody(shape));
}

static void removeAndSpawnFruit(cpSpace *space, cpShape *shape, CallbackProp *new)
{
    cpSpaceRemoveShape(space, shape);
    cpSpaceRemoveBody(space, cpShapeGetBody(shape));
    if (head < 0)
        head = linkedlistAdd(NULL, create_ball(getSpriteId(), new->gfx_id, new->pos.x, new->pos.y));
    else
        head = linkedlistAdd(&head, create_ball(getSpriteId(), new->gfx_id, new->pos.x, new->pos.y));
    cpBody *body = cpShapeGetBody(head->data);
    cpSpaceAddBody(space, body);
    //cpBodySetPosition(body, cpv(new->pos.x, new->pos.y));
    cpSpaceAddShape(space, head->data);
}

static cpBool combineFruits(cpArbiter *arb, cpSpace *space, void *data){
    CP_ARBITER_GET_SHAPES(arb, a, b);
    FruitProperties *aFruitProp, *bFruitProp;
    aFruitProp = cpShapeGetUserData(a);
    bFruitProp = cpShapeGetUserData(b);
    if (aFruitProp->diameter == bFruitProp->diameter && aFruitProp->diameter < 64 && aFruitProp->id != -1 && bFruitProp->id != -1)
    {
        NF_DeleteSprite(0, aFruitProp->id);
        NF_DeleteSprite(0, bFruitProp->id);
        if (aFruitProp->bottom)
            NF_DeleteSprite(1, aFruitProp->id);
        if (bFruitProp->bottom)
            NF_DeleteSprite(1, bFruitProp->id);
        
        taken_sprites[aFruitProp->id] = 1;
        taken_sprites[bFruitProp->id] = 1;
        
        aFruitProp->id = -1;
        bFruitProp->id = -1;

        cpVect aPos = cpBodyGetPosition(aFruitProp->body);
        cpVect bPos = cpBodyGetPosition(bFruitProp->body);
        cpVect newPos = cpv((aPos.x + bPos.x) / 2, (aPos.y + bPos.y) / 2);

        CallbackProp *prop = malloc(sizeof *prop);
        *prop = (CallbackProp) {.gfx_id = aFruitProp->gfx_id+1, .pos = newPos};

        cpSpaceAddPostStepCallback(space, (cpPostStepFunc)removeFruit, a, NULL);
        cpSpaceAddPostStepCallback(space, (cpPostStepFunc)removeAndSpawnFruit, b, prop);

        return cpFalse;
    }
    else
    return cpTrue;
}

int main(int argc, char **argv)
{
    NF_Set2D(0, 0);
    NF_Set2D(1, 0);

    srand(time(NULL));

    //consoleDemoInit(); // Initialize console
    consoleDebugInit(DebugDevice_NOCASH);

    swiWaitForVBlank();

    // Initialize NitroFS and set it as the root folder of the filesystem
    nitroFSInit(NULL);
    NF_SetRootFolder("NITROFS");

    // Initialize tiled backgrounds system
    NF_InitTiledBgBuffers();    // Initialize storage buffers
    NF_InitTiledBgSys(0);       // Top screen
    NF_InitTiledBgSys(1);       // Bottom screen

    // Initialize sprite system
    NF_InitSpriteBuffers();     // Initialize storage buffers
    NF_InitSpriteSys(0);        // Top screen
    NF_InitSpriteSys(1);        // Bottom screen

    LoadAssets();

    uint16 keys_held, keys_down;
    memset(taken_sprites, 1, sizeof(taken_sprites));
    s16 actual_x = SCREEN_WIDTH/2, actual_y = 16;
    s8 sinceLastDrop = 121;

    s16 render_x, render_y;
    cpVect pos, vel;
    FruitProperties *activeFruitProp, *fruitProp;
    cpBody *tempBody;

    // Initialize physics
    cpVect gravity = cpv(0, 100);
    cpSpace *space = cpSpaceNew();
    cpSpaceSetGravity(space, gravity);
    cpSpaceSetIterations(space, 3);
    cpSpaceSetDamping(space, 0.7f);
    //cpSpaceSetIdleSpeedThreshold(space, 1); // Default 0
    cpSpaceSetSleepTimeThreshold(space, SLEEP_TIME_THRESHOLD);
    cpSpaceSetCollisionSlop(space, 1);
    //cpSpaceSetCollisionBias(space, cpfpow(1.0f - 0.1f, 60.0f));

    // Static bodies
    cpShape *floor = cpSegmentShapeNew(cpSpaceGetStaticBody(space), cpv(0, DOUBLE_SCREEN_HEIGHT), cpv(SCREEN_WIDTH, DOUBLE_SCREEN_HEIGHT), 0);
    cpShape *left_wall = cpSegmentShapeNew(cpSpaceGetStaticBody(space), cpv(0, DOUBLE_SCREEN_HEIGHT), cpv(0, 0), 0);
    cpShape *right_wall = cpSegmentShapeNew(cpSpaceGetStaticBody(space), cpv(SCREEN_WIDTH, DOUBLE_SCREEN_HEIGHT), cpv(SCREEN_WIDTH, 0), 0);
    cpShapeSetFriction(floor, 1);
    cpShapeSetFriction(left_wall, 1);
    cpShapeSetFriction(right_wall, 1);
    cpSpaceAddShape(space, floor);
    cpSpaceAddShape(space, left_wall);
    cpSpaceAddShape(space, right_wall);

    cpFloat timeStep = 1.0/TIME_STEP;

    cpCollisionHandler *handler = cpSpaceAddCollisionHandler(space, BALL_COLLISION, BALL_COLLISION);
    handler->preSolveFunc = combineFruits; // move all the params into callback for this lol..

    cpShape *actual, *next;
    actual = create_ball(getSpriteId(), 2, -256, -256);
    next = create_ball(getSpriteId(), 2, -256, -256);

    while (1)
    {
        scanKeys();
        keys_held = keysHeld();
        keys_down = keysDown();

        switch (keys_down)
        {
            case KEY_DOWN:
                if (sinceLastDrop > ALLOW_DROP)
                {
                    activeFruitProp = cpShapeGetUserData(actual);
                    cpSpaceAddBody(space, activeFruitProp->body);
                    cpBodySetPosition(activeFruitProp->body, cpv(actual_x, actual_y));
                    cpSpaceAddShape(space, actual);
                    if (head < 0)
                        head = linkedlistAdd(NULL, actual);
                    else
                        head = linkedlistAdd(&head, actual);
                    actual = next;
                    next = create_ball(getSpriteId(), rand() % (TOTAL_FRUITS - 3), -256, -256); // rand() % (TOTAL_FRUITS - 3)
                    sinceLastDrop = 0;
                }
                break;
            default:
                break;
        }

        switch (keys_held)
        {
            case KEY_LEFT:
                actual_x -= 4;
                break;
            case KEY_RIGHT:
                actual_x += 4;  
                break;
            default:
                break;
        }

        activeFruitProp = cpShapeGetUserData(actual);

        if (actual_x < activeFruitProp->radius)
            actual_x = activeFruitProp->radius;
        else if (actual_x > SCREEN_WIDTH - activeFruitProp->radius)
            actual_x = SCREEN_WIDTH - activeFruitProp->radius;


        // Render in-game fruits
        LinkedList *current = head;
        while(current != NULL)
        {
            fruitProp = cpShapeGetUserData(current->data);
            if (fruitProp->id != -1)
            {
                pos = cpBodyGetPosition(fruitProp->body);
                // vel = cpBodyGetVelocity(fruitProp->body);
                // if (vel.x < 1.f && vel.x > -1.f)
                //     cpBodySetVelocity(balls[i].body, cpv(0, vel.y));

                render_x = pos.x - fruitProp->sprite_size_half;
                render_y = pos.y - fruitProp->sprite_size_half;
                if (render_y < SCREEN_HEIGHT)
                {
                    NF_MoveSprite(0, fruitProp->id, render_x, render_y);
                    if (render_y + fruitProp->diameter > SCREEN_HEIGHT)
                    {
                            if (fruitProp->bottom)
                                NF_MoveSprite(1, fruitProp->id, render_x, render_y - SCREEN_HEIGHT);
                            else
                            {
                                NF_CreateSprite(1, fruitProp->id, fruitProp->gfx_id, fruitProp->gfx_id, render_x, render_y - SCREEN_HEIGHT);
                                fruitProp->bottom = true;
                            }
                    }
                    //balls[i].hideFromTop = !balls[i].hideFromTop;
                }
                else
                {
                    if (fruitProp->bottom)
                        NF_MoveSprite(1, fruitProp->id, render_x, render_y - SCREEN_HEIGHT);
                    else
                    {
                        NF_CreateSprite(1, fruitProp->id, fruitProp->gfx_id, fruitProp->gfx_id, render_x, render_y - SCREEN_HEIGHT);
                        fruitProp->bottom = true;
                    }

                    if (!fruitProp->hideFromTop)
                    {
                        NF_MoveSprite(0, fruitProp->id, -256, -256);
                        fruitProp->hideFromTop = true;
                    }
                }
                current = current->next; // safe to do if nothing to remove
            }
            else
            {
                tempBody = cpShapeGetBody(current->data);
                cpShapeFree(current->data);
                cpBodyFree(tempBody);
                if (current == head)
                {
                    head = current->next;
                    linkedlistRemove(current);
                    current = head; // go from new head
                    
                }
                else // removing non-head
                {
                    LinkedList* newCurrent = current->next; // keeping actual next
                    linkedlistRemove(current);
                    current = newCurrent;
                }
            }
            
        }

        // Simulate world
        cpSpaceStep(space, timeStep);

        if (sinceLastDrop <= ALLOW_DROP)
            sinceLastDrop++;

        // Update position of upper fruit

        NF_MoveSprite(0, activeFruitProp->id, actual_x - activeFruitProp->sprite_size_half, actual_y - activeFruitProp->sprite_size_half);

        // Update OAM array
        NF_SpriteOamSet(0);
        NF_SpriteOamSet(1);
        // Wait for the screen refresh
        swiWaitForVBlank();
        // Update OAM
        oamUpdate(&oamMain);
        oamUpdate(&oamSub);
        
    }

    // If this is reached, the program will return to the loader if the loader
    // supports it.
    return 0;
}