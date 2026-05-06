/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "../SDL_internal.h"
#include "SDL_events.h"
#ifndef SDL_GESTURES_DISABLED
/* General gesture handling code for SDL */
#include "SDL_endian.h"
#include "SDL_events_c.h"
#include "SDL_gesture_c.h"

/*
#include <stdio.h>
*/

/* TODO: Replace with SDL_malloc */

#define MAXPATHSIZE 1024

#define ENABLE_DOLLAR

#define DOLLARNPOINTS 64

#if defined(ENABLE_DOLLAR)
#define DOLLARSIZE 256
#define PHI        0.618033989
#endif

typedef struct
{
    float x, y;
} SDL_FloatPoint;

typedef struct
{
    int numPoints;
    SDL_FloatPoint p[MAXPATHSIZE];
} SDL_DollarPath;

typedef struct
{
    SDL_FloatPoint path[DOLLARNPOINTS];
    Sint64 hash;
} SDL_DollarTemplate;

typedef struct
{
    SDL_TouchID id;
    SDL_FloatPoint centroid;
    SDL_DollarPath dollarPath;
    Uint16 numDownFingers;

    int numDollarTemplates;
    SDL_DollarTemplate *dollarTemplate;

    int recording;
    int recordAll;
} SDL_GestureTouch;

static SDL_GestureTouch *SDL_gestureTouch;
static int SDL_numGestureTouches = 0;

#if 0
static void PrintPath(const SDL_FloatPoint *path)
{
    int i;
    printf("Path:");
    for (i=0; i<DOLLARNPOINTS; i++) {
        printf(" (%f,%f)",path[i].x,path[i].y);
    }
    printf("\n");
}
#endif

int SDL_RecordGesture(SDL_TouchID touchId)
{
    int i;
    for (i = 0; i < SDL_numGestureTouches; i++) {
        if ((touchId < 0) || (SDL_gestureTouch[i].id == touchId)) {
            SDL_gestureTouch[i].recording++;
            if (touchId >= 0) {
                return 1;
            }
            SDL_gestureTouch[i].recordAll++;
        }
    }
    return touchId < 0;
}

void SDL_GestureQuit(void)
{
    SDL_assert(SDL_numGestureTouches == 0);
    SDL_free(SDL_gestureTouch);
    SDL_gestureTouch = NULL;
}

typedef union float_bits {
    Uint32 u32;
    float f32;
} float_bits;
static Uint64 SDL_HashDollar(const SDL_FloatPoint *points)
{
    Uint64 hash = 5381;
    int i;
    for (i = 0; i < DOLLARNPOINTS; i++) {
        float_bits pos;
        pos.f32 = points[i].x;
        hash = ((hash << 5) + hash) + pos.u32;
        pos.f32 = points[i].y;
        hash = ((hash << 5) + hash) + pos.u32;
    }
    return hash;
}
#ifndef SDL_FILE_DISABLED
static int SaveTemplate(const SDL_DollarTemplate *templ, SDL_RWops *dst)
{
    if (!dst) {
        return 0;
    }

    /* No Longer storing the Hash, rehash on load */
    /* if (SDL_RWops.write(dst, &(templ->hash), sizeof(templ->hash), 1) != 1) return 0; */

    {
#if SDL_BYTEORDER != SDL_LIL_ENDIAN
        SDL_DollarTemplate copy = *templ;
        SDL_FloatPoint *p = copy.path;
        int i;
        for (i = 0; i < DOLLARNPOINTS; i++, p++) {
            p->x = SDL_SwapFloatLE(p->x);
            p->y = SDL_SwapFloatLE(p->y);
        }
        templ = &copy;
#endif
        if (SDL_RWwrite(dst, templ->path,
                        sizeof(templ->path[0]), DOLLARNPOINTS) != DOLLARNPOINTS) {
            return 0;
        }
    }

    return 1;
}
#endif /* !SDL_FILE_DISABLED */
int SDL_SaveAllDollarTemplates(SDL_RWops *dst)
{
#ifdef SDL_FILE_DISABLED
    return SDL_SetError("Unsupported, because SDL2 is compiled without FILE subsystem");
#else
    int i, j, rtrn = 0;
    for (i = 0; i < SDL_numGestureTouches; i++) {
        SDL_GestureTouch *touch = &SDL_gestureTouch[i];
        for (j = 0; j < touch->numDollarTemplates; j++) {
            rtrn += SaveTemplate(&touch->dollarTemplate[j], dst);
        }
    }
    return rtrn;
#endif /* SDL_FILE_DISABLED */
}

int SDL_SaveDollarTemplate(SDL_GestureID gestureId, SDL_RWops *dst)
{
#ifdef SDL_FILE_DISABLED
    return SDL_SetError("Unsupported, because SDL2 is compiled without FILE subsystem");
#else
    int i, j;
    for (i = 0; i < SDL_numGestureTouches; i++) {
        SDL_GestureTouch *touch = &SDL_gestureTouch[i];
        for (j = 0; j < touch->numDollarTemplates; j++) {
            if (touch->dollarTemplate[j].hash == gestureId) {
                return SaveTemplate(&touch->dollarTemplate[j], dst);
            }
        }
    }
    return SDL_SetError("Unknown gestureId");
#endif /* SDL_FILE_DISABLED */
}

/* path is an already sampled set of points
Returns the index of the gesture on success, or -1 */
static int SDL_AddDollarGesture_one(SDL_GestureTouch *inTouch, const SDL_FloatPoint *path)
{
    SDL_DollarTemplate *dollarTemplate;
    SDL_DollarTemplate *templ;
    int index;

    index = inTouch->numDollarTemplates;
    dollarTemplate =
        (SDL_DollarTemplate *)SDL_realloc(inTouch->dollarTemplate,
                                          (index + 1) *
                                              sizeof(SDL_DollarTemplate));
    if (!dollarTemplate) {
        return SDL_OutOfMemory();
    }
    inTouch->dollarTemplate = dollarTemplate;

    templ = &inTouch->dollarTemplate[index];
    SDL_memcpy(templ->path, path, DOLLARNPOINTS * sizeof(SDL_FloatPoint));
    templ->hash = SDL_HashDollar(templ->path);
    inTouch->numDollarTemplates++;

    return index;
}

static int SDL_AddDollarGesture(SDL_GestureTouch *inTouch, const SDL_FloatPoint *path)
{
    int index;
    int i = 0;
    if (!inTouch) {
        if (SDL_numGestureTouches <= 0) {
            return SDL_SetError("no gesture touch devices registered");
        }
        for (i = 0; i < SDL_numGestureTouches; i++) {
            inTouch = &SDL_gestureTouch[i];
            index = SDL_AddDollarGesture_one(inTouch, path);
            if (index < 0) {
                return index;
            }
        }
        /* Use the index of the last one added. */
        return index;
    }
    return SDL_AddDollarGesture_one(inTouch, path);
}

static SDL_GestureTouch *SDL_GetGestureTouch(SDL_TouchID id)
{
    int i;
    for (i = 0; i < SDL_numGestureTouches; i++) {
        /* printf("%i ?= %i\n",SDL_gestureTouch[i].id,id); */
        if (SDL_gestureTouch[i].id == id) {
            return &SDL_gestureTouch[i];
        }
    }
    return NULL;
}

int SDL_LoadDollarTemplates(SDL_TouchID touchId, SDL_RWops *src)
{
#ifdef SDL_FILE_DISABLED
    return SDL_SetError("Unsupported, because SDL2 is compiled without FILE subsystem");
#else
    int loaded = 0;
    SDL_GestureTouch *touch = NULL;
    if (!src) {
        return 0;
    }
    if (touchId >= 0) {
        touch = SDL_GetGestureTouch(touchId);
        if (!touch) {
            return SDL_SetError("given touch id not found");
        }
    }

    while (1) {
        SDL_DollarTemplate templ;

        if (SDL_RWread(src, templ.path, sizeof(templ.path[0]), DOLLARNPOINTS) < DOLLARNPOINTS) {
            if (loaded == 0) {
                return SDL_SetError("could not read any dollar gesture from rwops");
            }
            break;
        }

#if SDL_BYTEORDER != SDL_LIL_ENDIAN
        for (int i = 0; i < DOLLARNPOINTS; i++) {
            SDL_FloatPoint *p = &templ.path[i];
            p->x = SDL_SwapFloatLE(p->x);
            p->y = SDL_SwapFloatLE(p->y);
        }
#endif

        if (SDL_AddDollarGesture(touch, templ.path) >= 0) {
            loaded++;
        }
    }

    return loaded;
#endif /* SDL_FILE_DISABLED */
}

#if defined(ENABLE_DOLLAR)
static void SDL_SendGestureDollar(const SDL_GestureTouch *touch,
                                  SDL_GestureID gestureId, float error)
{
    if (SDL_IsEventEnabled(SDL_DOLLARGESTURE)) {
        SDL_Event event;
        event.dgesture.type = SDL_DOLLARGESTURE;
        event.dgesture.touchId = touch->id;
        event.dgesture.x = touch->centroid.x;
        event.dgesture.y = touch->centroid.y;
        event.dgesture.gestureId = gestureId;
        event.dgesture.error = error;
        /* A finger came up to trigger this event. */
        event.dgesture.numFingers = touch->numDownFingers + 1;
        SDL_PushEvent(&event);
    }
}

static void SDL_SendDollarRecord(const SDL_GestureTouch *touch, SDL_GestureID gestureId)
{
    if (SDL_IsEventEnabled(SDL_DOLLARRECORD)) {
        SDL_Event event;
        event.dgesture.type = SDL_DOLLARRECORD;
        event.dgesture.touchId = touch->id;
        event.dgesture.gestureId = gestureId;
        SDL_PushEvent(&event);
    }
}

static float dollarDifference(const SDL_FloatPoint *points, const SDL_FloatPoint *templ, float ang)
{
    /*  SDL_FloatPoint p[DOLLARNPOINTS]; */
    float dist = 0;
    SDL_FloatPoint p;
    int i;
    for (i = 0; i < DOLLARNPOINTS; i++) {
        p.x = points[i].x * SDL_cosf(ang) - points[i].y * SDL_sinf(ang);
        p.y = points[i].x * SDL_sinf(ang) + points[i].y * SDL_cosf(ang);
        dist += SDL_sqrtf((p.x - templ[i].x) * (p.x - templ[i].x) +
                          (p.y - templ[i].y) * (p.y - templ[i].y));
    }
    return dist / DOLLARNPOINTS;
}

static float bestDollarDifference(const SDL_FloatPoint *points, const SDL_FloatPoint *templ)
{
    /*------------BEGIN DOLLAR BLACKBOX------------------
      -TRANSLATED DIRECTLY FROM PSUDEO-CODE AVAILABLE AT-
      -"http://depts.washington.edu/aimgroup/proj/dollar/"
    */
    double ta = -M_PI/4;
    double tb = M_PI/4;
    double dt = M_PI/90;
    float x1 = (float)(PHI*ta + (1-PHI)*tb);
    float f1 = dollarDifference(points,templ,x1);
    float x2 = (float)((1-PHI)*ta + PHI*tb);
    float f2 = dollarDifference(points,templ,x2);
    while (SDL_fabs(ta-tb) > dt) {
        if (f1 < f2) {
            tb = x2;
            x2 = x1;
            f2 = f1;
            x1 = (float)(PHI * ta + (1 - PHI) * tb);
            f1 = dollarDifference(points, templ, x1);
        } else {
            ta = x1;
            x1 = x2;
            f1 = f2;
            x2 = (float)((1 - PHI) * ta + PHI * tb);
            f2 = dollarDifference(points, templ, x2);
        }
    }
    /*
      if (f1 <= f2)
          printf("Min angle (x1): %f\n",x1);
      else if (f1 >  f2)
          printf("Min angle (x2): %f\n",x2);
    */
    return SDL_min(f1, f2);
}

/* DollarPath contains raw points, plus (possibly) the calculated length */
static int dollarNormalize(const SDL_DollarPath *path, SDL_FloatPoint *points)
{
    int i;
    float interval;
    float dist;
    int numPoints = 0;
    SDL_FloatPoint centroid;
    float xmin, xmax, ymin, ymax;
    float ang;
    float w, h;
    float length = 0.0f;

    /* Calculate length */
    {
        for (i = 1; i < path->numPoints; i++) {
            float dx = path->p[i].x - path->p[i - 1].x;
            float dy = path->p[i].y - path->p[i - 1].y;
            length += SDL_sqrtf(dx * dx + dy * dy);
        }
    }

    /* Resample */
    interval = length / (DOLLARNPOINTS - 1);
    dist = interval;

    centroid.x = 0;
    centroid.y = 0;

    /* printf("(%f,%f)\n",path->p[path->numPoints-1].x,path->p[path->numPoints-1].y); */
    for (i = 1; i < path->numPoints; i++) {
        float dx = path->p[i].x - path->p[i - 1].x;
        float dy = path->p[i].y - path->p[i - 1].y;
        float d = SDL_sqrtf(dx * dx + dy * dy);
        /* printf("d = %f dist = %f/%f\n",d,dist,interval); */
        while (dist + d > interval) {
            points[numPoints].x = path->p[i - 1].x +
                                  ((interval - dist) / d) * dx;
            points[numPoints].y = path->p[i - 1].y +
                                  ((interval - dist) / d) * dy;
            centroid.x += points[numPoints].x;
            centroid.y += points[numPoints].y;
            numPoints++;

            dist -= interval;
        }
        dist += d;
    }
    if (numPoints < DOLLARNPOINTS - 1) {
        return -1;
    }
    /* copy the last point */
    points[DOLLARNPOINTS - 1] = path->p[path->numPoints - 1];
    numPoints = DOLLARNPOINTS;

    centroid.x /= numPoints;
    centroid.y /= numPoints;

    /* printf("Centroid (%f,%f)",centroid.x,centroid.y); */
    /* Rotate Points so point 0 is left of centroid and solve for the bounding box */
    xmin = centroid.x;
    xmax = centroid.x;
    ymin = centroid.y;
    ymax = centroid.y;

    ang = SDL_atan2f(centroid.y - points[0].y,
                     centroid.x - points[0].x);

    for (i = 0; i < numPoints; i++) {
        float px = points[i].x;
        float py = points[i].y;
        points[i].x = (px - centroid.x) * SDL_cosf(ang) -
                              (py - centroid.y) * SDL_sinf(ang) + centroid.x;
        points[i].y = (px - centroid.x) * SDL_sinf(ang) +
                              (py - centroid.y) * SDL_cosf(ang) + centroid.y;

        if (points[i].x < xmin) {
            xmin = points[i].x;
        }
        if (points[i].x > xmax) {
            xmax = points[i].x;
        }
        if (points[i].y < ymin) {
            ymin = points[i].y;
        }
        if (points[i].y > ymax) {
            ymax = points[i].y;
        }
    }

    /* Scale points to DOLLARSIZE, and translate to the origin */
    w = xmax - xmin;
    h = ymax - ymin;

    for (i = 0; i < numPoints; i++) {
        points[i].x = (points[i].x - centroid.x) * DOLLARSIZE / w;
        points[i].y = (points[i].y - centroid.y) * DOLLARSIZE / h;
    }
    return 0;
}
#endif

void SDL_GestureAddTouch(SDL_TouchID touchId)
{
    SDL_GestureTouch *gestureTouch = (SDL_GestureTouch *)SDL_realloc(SDL_gestureTouch,
                                                                     (SDL_numGestureTouches + 1) *
                                                                         sizeof(SDL_GestureTouch));

    if (!gestureTouch) {
        return; // SDL_OutOfMemory();
    }

    SDL_gestureTouch = gestureTouch;

    SDL_zero(SDL_gestureTouch[SDL_numGestureTouches]);
    SDL_gestureTouch[SDL_numGestureTouches].id = touchId;
    SDL_numGestureTouches++;
}

void SDL_GestureDelTouch(SDL_TouchID touchId)
{
    SDL_GestureTouch *touch = SDL_GetGestureTouch(touchId);
    SDL_GestureTouch *lastTouch;
    if (!touch) {
        /* not found */
        return;
    }

    SDL_free(touch->dollarTemplate);
    SDL_zero(*touch);

    SDL_numGestureTouches--;
    lastTouch = &SDL_gestureTouch[SDL_numGestureTouches];
    if (touch != lastTouch) {
        SDL_copyp(touch, lastTouch);
    }
}

static void SDL_SendGestureMulti(const SDL_GestureTouch *touch, float dTheta, float dDist)
{
    if (SDL_IsEventEnabled(SDL_MULTIGESTURE)) {
        SDL_Event event;
        event.mgesture.type = SDL_MULTIGESTURE;
        event.mgesture.touchId = touch->id;
        event.mgesture.x = touch->centroid.x;
        event.mgesture.y = touch->centroid.y;
        event.mgesture.dTheta = dTheta;
        event.mgesture.dDist = dDist;
        event.mgesture.numFingers = touch->numDownFingers;
        SDL_PushEvent(&event);
    }
}

void SDL_GestureProcessEvent(SDL_Event *event)
{
    if (event->type == SDL_FINGERMOTION ||
        event->type == SDL_FINGERDOWN ||
        event->type == SDL_FINGERUP) {
        SDL_GestureTouch *inTouch = SDL_GetGestureTouch(event->tfinger.touchId);
        float x, y;

        /* Shouldn't be possible */
        if (!inTouch) {
            return;
        }

        x = event->tfinger.x;
        y = event->tfinger.y;

        /* Finger Up */
        if (event->type == SDL_FINGERUP) {
            inTouch->numDownFingers--;

#if defined(ENABLE_DOLLAR)
            if (inTouch->recording || inTouch->numDollarTemplates) {
                int i;
                SDL_FloatPoint points[DOLLARNPOINTS];
                if (dollarNormalize(&inTouch->dollarPath, points) >= 0) {
                    if (inTouch->recording) {
                        /* PrintPath(points); */
                        int index;
                        if (inTouch->recordAll) {
                            index = SDL_AddDollarGesture(NULL, points);
                            for (i = 0; i < SDL_numGestureTouches; i++) {
                                SDL_gestureTouch[i].recording--;
                                SDL_gestureTouch[i].recordAll--;
                            }
                        } else {
                            index = SDL_AddDollarGesture(inTouch, points);
                            inTouch->recording--;
                        }

                        SDL_SendDollarRecord(inTouch, index >= 0 ? inTouch->dollarTemplate[index].hash : -1);
                    } else {
                        float minDiff = 10000;
                        const SDL_DollarTemplate *bestTempl = NULL;
                        /* PrintPath(points); */
                        for (i = 0; i < inTouch->numDollarTemplates; i++) {
                            const SDL_DollarTemplate *templ = &inTouch->dollarTemplate[i];
                            float diff = bestDollarDifference(points, templ->path);
                            if (diff < minDiff) {
                                minDiff = diff;
                                bestTempl = templ;
                            }
                        }
                        /* Send Event */
                        if (bestTempl != NULL) {
                            Sint64 gestureId = bestTempl->hash;
                            SDL_SendGestureDollar(inTouch, gestureId, minDiff);
                            /* printf ("%s\n",);("Dollar error: %f\n",minDiff); */
                        }
                    }
                } else {
                    if (inTouch->recording && inTouch->numDownFingers == 0) {
                        SDL_SetError("Empty path.");
                    }
                }
            }
#endif
            /* inTouch->gestureLast[j] = inTouch->gestureLast[inTouch->numDownFingers]; */
            if (inTouch->numDownFingers > 0) {
                inTouch->centroid.x = (inTouch->centroid.x * (inTouch->numDownFingers + 1) -
                                       x) /
                                      inTouch->numDownFingers;
                inTouch->centroid.y = (inTouch->centroid.y * (inTouch->numDownFingers + 1) -
                                       y) /
                                      inTouch->numDownFingers;
            }
        } else if (event->type == SDL_FINGERMOTION) {
            float dx, dy;
            SDL_FloatPoint lastP;
            SDL_FloatPoint lastCentroid;
#if defined(ENABLE_DOLLAR)
            SDL_DollarPath *path = &inTouch->dollarPath;
            if (path->numPoints < MAXPATHSIZE) {
                SDL_copyp(&path->p[path->numPoints], &inTouch->centroid);
                path->numPoints++;
            }
#endif
            dx = event->tfinger.dx;
            dy = event->tfinger.dy;
            lastP.x = x - dx;
            lastP.y = y - dy;
            lastCentroid = inTouch->centroid;

            inTouch->centroid.x += dx / inTouch->numDownFingers;
            inTouch->centroid.y += dy / inTouch->numDownFingers;
            /* printf("Centrid : (%f,%f)\n",inTouch->centroid.x,inTouch->centroid.y); */
            if (inTouch->numDownFingers > 1) {
                SDL_FloatPoint lv; /* Vector from centroid to last x,y position */
                SDL_FloatPoint v;  /* Vector from centroid to current x,y position */
                float lDist, Dist;
                float dDist, dtheta;
                /* lv = inTouch->gestureLast[j].cv; */
                lv.x = lastP.x - lastCentroid.x;
                lv.y = lastP.y - lastCentroid.y;
                lDist = SDL_sqrtf(lv.x * lv.x + lv.y * lv.y);
                /* printf("lDist = %f\n",lDist); */
                v.x = x - inTouch->centroid.x;
                v.y = y - inTouch->centroid.y;
                /* inTouch->gestureLast[j].cv = v; */
                Dist = SDL_sqrtf(v.x * v.x + v.y * v.y);
                /* SDL_cos(dTheta) = (v . lv)/(|v| * |lv|) */
#if 1
                SDL_INLINE_COMPILE_TIME_ASSERT(gesture_cmp_zero, sizeof(Dist) == sizeof(int) && sizeof(lDist) == sizeof(int));
                if (*((int*)(&Dist)) != 0 && *((int*)(&lDist)) != 0) {
#else
                if (Dist != 0 && lDist != 0) {
#endif
                    dDist = (Dist - lDist);
                    dtheta = SDL_atan2f(lv.x * v.y - lv.y * v.x, lv.x * v.x + lv.y * v.y);
                } else {
                    /* To avoid impossible values */
                    dDist = 0;
                    dtheta = 0;
                }

                /* inTouch->gestureLast[j].dDist = dDist;
                inTouch->gestureLast[j].dtheta = dtheta;

                printf("dDist = %f, dTheta = %f\n",dDist,dtheta);
                gdtheta = gdtheta*.9 + dtheta*.1;
                gdDist  =  gdDist*.9 +  dDist*.1
                knob.r += dDist/numDownFingers;
                knob.ang += dtheta;
                printf("thetaSum = %f, distSum = %f\n",gdtheta,gdDist);
                printf("id: %i dTheta = %f, dDist = %f\n",j,dtheta,dDist); */
                SDL_SendGestureMulti(inTouch, dtheta, dDist);
            } else {
                /* inTouch->gestureLast[j].dDist = 0;
                inTouch->gestureLast[j].dtheta = 0;
                inTouch->gestureLast[j].cv.x = 0;
                inTouch->gestureLast[j].cv.y = 0; */
            }
            /* inTouch->gestureLast[j].f.p.x = x;
            inTouch->gestureLast[j].f.p.y = y;
            break;
            pressure? */
        } else if (event->type == SDL_FINGERDOWN) {

            inTouch->numDownFingers++;
            inTouch->centroid.x = (inTouch->centroid.x * (inTouch->numDownFingers - 1) +
                                   x) /
                                  inTouch->numDownFingers;
            inTouch->centroid.y = (inTouch->centroid.y * (inTouch->numDownFingers - 1) +
                                   y) /
                                  inTouch->numDownFingers;
            /* printf("Finger Down: (%f,%f). Centroid: (%f,%f\n",x,y,
                 inTouch->centroid.x,inTouch->centroid.y); */

#if defined(ENABLE_DOLLAR)
            inTouch->dollarPath.p[0].x = x;
            inTouch->dollarPath.p[0].y = y;
            inTouch->dollarPath.numPoints = 1;
#endif
        }
    }
}
#else
void SDL_GestureAddTouch(SDL_TouchID touchId)
{
}
void SDL_GestureDelTouch(SDL_TouchID touchId)
{
}

void SDL_GestureProcessEvent(SDL_Event *event)
{
}

void SDL_GestureQuit(void)
{
}

int SDL_RecordGesture(SDL_TouchID touchId)
{
    return SDL_SetError("Unsupported, because SDL2 is compiled without gestures support");
}

int SDL_SaveAllDollarTemplates(SDL_RWops *dst)
{
    return SDL_SetError("Unsupported, because SDL2 is compiled without gestures support");
}

int SDL_SaveDollarTemplate(SDL_GestureID gestureId, SDL_RWops *dst)
{
    return SDL_SetError("Unsupported, because SDL2 is compiled without gestures support");
}

int SDL_LoadDollarTemplates(SDL_TouchID touchId, SDL_RWops *src)
{
    return SDL_SetError("Unsupported, because SDL2 is compiled without gestures support");
}
#endif // SDL_GESTURES_DISABLED

/* vi: set ts=4 sw=4 expandtab: */
