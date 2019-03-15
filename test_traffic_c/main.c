#include "libabl.h"
#include "time.h"
#include "stdio.h"
typedef struct {
    double velocity;
    float2 pos;
    int id;
    bool isFinished;
} Point;
static const type_info Point_info[] = {
    { TYPE_FLOAT, offsetof(Point, velocity), "velocity", false },
    { TYPE_FLOAT2, offsetof(Point, pos), "pos", true },
    { TYPE_INT, offsetof(Point, id), "id", false },
    { TYPE_BOOL, offsetof(Point, isFinished), "isFinished", false },
    { TYPE_END, sizeof(Point), NULL }
};

typedef struct {
    int laneId;
    double length;
    int speedLimit;
    int nextLaneId;
    int leftLaneId;
    int rightLaneId;
} Lane;

Lane roads[2048];


struct agent_struct {
    dyn_array agents_Point;
    dyn_array agents_Point_dbuf;
};
struct agent_struct agents;
static const agent_info agents_info[] = {
    { Point_info, offsetof(struct agent_struct, agents_Point), "Point" },
    { NULL, 0, NULL }
};

double PI = 3.14159;
int num_timesteps = 100;
int num_agents = 8192;
double dt = 250.0;
double size = 3.5;
float2 random_1(float2 min, float2 max) {
    return float2_create(random_float(min.x, max.x), random_float(min.y, max.y));
}
float3 random_2(float3 min, float3 max) {
    return float3_create(random_float(min.x, max.x), random_float(min.y, max.y), random_float(min.z, max.z));
}
double random_3(double max) {
    return random_float(0, max);
}
float2 random_4(float2 max) {
    return random_1(float2_fill(0), max);
}
float3 random_5(float3 max) {
    return random_2(float3_fill(0), max);
}
int randomInt_1(int max) {
    return random_int(0, max);
}
double clam(double pos, double min, double max) {
    return ((pos < min) ? min : ((pos > max) ? max : pos));
}
float2 clam_1(float2 pos, float2 min, float2 max) {
    return float2_create(((pos.x < min.x) ? min.x : ((pos.x > max.x) ? max.x : pos.x)), ((pos.y < min.y) ? min.y : ((pos.y > max.y) ? max.y : pos.y)));
}
float3 clam_2(float3 pos, float3 min, float3 max) {
    return float3_create(((pos.x < min.x) ? min.x : ((pos.x > max.x) ? max.x : pos.x)), ((pos.y < min.y) ? min.y : ((pos.y > max.y) ? max.y : pos.y)), ((pos.z < min.z) ? min.z : ((pos.z > max.z) ? max.z : pos.z)));
}
double clam_3(double pos, double max) {
    return clam(pos, 0, max);
}
float2 clam_4(float2 pos, float2 max) {
    return clam_1(pos, float2_fill(0), max);
}
float3 clam_5(float3 pos, float3 max) {
    return clam_2(pos, float3_fill(0), max);
}
double wraparound(double pos, double max) {
    return ((pos < 0) ? (max + pos) : ((pos >= max) ? (pos - max) : pos));
}
float2 wraparound_1(float2 pos, float2 max) {
    return float2_create(((pos.x < 0) ? (max.x + pos.x) : ((pos.x >= max.x) ? (pos.x - max.x) : pos.x)), ((pos.y < 0) ? (max.y + pos.y) : ((pos.y >= max.y) ? (pos.y - max.y) : pos.y)));
}
float3 wraparound_2(float3 pos, float3 max) {
    return float3_create(((pos.x < 0) ? (max.x + pos.x) : ((pos.x >= max.x) ? (pos.x - max.x) : pos.x)), ((pos.y < 0) ? (max.y + pos.y) : ((pos.y >= max.y) ? (pos.y - max.y) : pos.y)), ((pos.z < 0) ? (max.z + pos.z) : ((pos.z >= max.z) ? (pos.z - max.z) : pos.z)));
}
bool is_inside(float2 pos, float2 max) {
    return ((((pos.x >= 0) && (pos.y >= 0)) && (pos.x <= max.x)) && (pos.y <= max.y));
}
bool isSortNeeded(int p1, int p2) {
    if ((p1 > p2)) return true;
    return false;
}
bool isSortNeeded_1(float2 p1, float2 p2) {
    if ((p1.x > p2.x)) return true;
    if ((p1.x == p2.x)) {
        if ((p1.y > p2.y)) {
            return true;
        }
    }
    return false;
}
void car_follow(Point* in, Point* out) {
    double desired_velocity = (double) roads[in->pos.x].speedLimit;
    double ratio = 0.0;
    double free_road_term = 0.0;
    if ((in->velocity < desired_velocity)) {
        ratio = (in->velocity / desired_velocity);
        free_road_term = (1.8 * (1.0 - pow(ratio, 4)));
    } else {
        ratio = (desired_velocity / in->velocity);
        free_road_term = ((-2.0) * (1.0 - pow(ratio, 4)));
    }
    double interaction_term = 0.0;

    for (size_t _var2 = 0; _var2 < num_agents; _var2++) {
        Point* nx = DYN_ARRAY_GET(&agents.agents_Point, Point, _var2);
        {
            if ((nx->pos.x == in->pos.x) && (nx->pos.y > in->pos.y)) {
                double dV = (in->velocity - nx->velocity);
                double temp = ((in->velocity * 1.5) + ((in->velocity * dV) / 3.79));
                double ss = (2.0 + temp);
                double pos_dist = (nx->pos.y - in->pos.y);
                interaction_term = ((-1.8) * pow((ss / pos_dist), 2.0));
                break;
            }
        }
    }

    double acceleration = (free_road_term + interaction_term);
    out->velocity = (in->velocity + (acceleration * (dt / 1000.0)));
    if ((out->velocity < 0)) out->velocity = 0;
    out->pos.y = (in->pos.y + (out->velocity * (dt / 1000.0)));
    if ((out->pos.y > roads[out->pos.x].length)) {
        if ((roads[out->pos.x].nextLaneId != (-1))) {
            out->pos.x = roads[in->pos.x].nextLaneId;
            out->pos.y = (out->pos.y - roads[out->pos.x].length);
        } else {
            out->isFinished = true;
        }
    }
}
void lane_change(Point* in, Point* out) {
//    if ((in->pos.x != out->pos.x)) return;
    double dis_rear = (-50.0);
    double dis_front = 50.0;
    double dis_rear_left = (-50.0);
    double dis_front_left = 50.0;
    double dis_rear_right = (-50.0);
    double dis_front_right = 50.0;
    double vel_rear = 0.0;
    double vel_front = 30.0;
    double vel_rear_left = 0.0;
    double vel_front_left = 30.0;
    double vel_rear_right = 0.0;
    double vel_front_right = 30.0;
    double dis_tmp_right = 0.0;
    double dis_tmp_left = 0.0;
    double dis_tmp = 0.0;
    if ((roads[in->pos.x].leftLaneId != (-1))) {
        for (size_t _var8 = 0; _var8 < num_agents; _var8++) {
            Point* nx = DYN_ARRAY_GET(&agents.agents_Point, Point, _var8);
            if (nx->pos.x == roads[in->pos.x].leftLaneId)
            {
                dis_tmp_left = (nx->pos.y - in->pos.y);
                if (((dis_tmp_left > 0) && (dis_tmp_left < dis_front_left))) {
                    dis_front_left = (dis_tmp_left - size);
                    vel_front_left = nx->velocity;
                }
                if (((dis_tmp_left < 0) && (dis_tmp_left > dis_rear_left))) {
                    dis_rear_left = (dis_tmp_left + size);
                    vel_rear_left = nx->velocity;
                }
            }
        }
    }
    if ((roads[in->pos.x].rightLaneId != (-1))) {
        for (size_t _var11 = 0; _var11 < num_agents; _var11++) {
            Point* nx = DYN_ARRAY_GET(&agents.agents_Point, Point, _var11);
            if (nx->pos.x == roads[in->pos.x].rightLaneId)
            {
                dis_tmp_right = (nx->pos.y - in->pos.y);
                if (((dis_tmp_right > 0) && (dis_tmp_right < dis_front_right))) {
                    dis_front_right = (dis_tmp_right - size);
                    vel_front_right = nx->velocity;
                }
                if (((dis_tmp_right < 0) && (dis_tmp_right > dis_rear_right))) {
                    dis_rear_right = (dis_tmp_right + size);
                    vel_rear_right = nx->velocity;
                }
            }
        }
    }

    for (size_t _var14 = 0; _var14 < num_agents; _var14++) {
        Point* nx = DYN_ARRAY_GET(&agents.agents_Point, Point, _var14);
        if (nx->pos.x == in->pos.x)
        {
            dis_tmp = (nx->pos.y - in->pos.y);
            if (((dis_tmp > 0) && (dis_tmp < dis_front))) {
                dis_front = (dis_tmp - size);
                vel_front = nx->velocity;
            }
            if (((dis_tmp < 0) && (dis_tmp > dis_rear))) {
                dis_rear = (dis_tmp + size);
                vel_rear = nx->velocity;
            }
        }
    }
    double con_e = 2.71828;
    double a_l = (((25.0 * in->velocity) * (vel_front_left - in->velocity)) / (dis_front_left * dis_front_left));
    double a_c = (((25.0 * in->velocity) * (vel_front - in->velocity)) / (dis_front * dis_front));
    double a_r = (((25.0 * in->velocity) * (vel_front_right - in->velocity)) / (dis_front_right * dis_front_right));
    double p_a_l = pow(con_e, a_l);
    double p_a_c = pow(con_e, a_c);
    double p_a_r = pow(con_e, a_r);
    double p_base = ((p_a_l + p_a_c) + p_a_r);
    double p_l = (p_a_l / p_base);
    double p_c = (p_a_c / p_base);
    double p_r = (p_a_r / p_base);
    double a_rear_l = (((25.0 * vel_rear_left) * (in->velocity - vel_rear_left)) / (dis_rear_left * dis_rear_left));
    double a_rear_r = (((25.0 * vel_rear_right) * (in->velocity - vel_rear_right)) / (dis_rear_right * dis_rear_right));
    double aa = ((-0.78) * ((dis_front_right / in->velocity) - 0.7));
    double bb = ((-0.78) * (((-dis_rear_right) / in->velocity) - 0.7));
    double p_right_lead = (1.0 - pow(con_e, aa));
    double p_right_lag = (1.0 - pow(con_e, bb));
    double p_right = (p_right_lead * p_right_lag);
    if (((dis_rear_right > (-size)) || (dis_front_right < size))) p_right = 0;
    double cc = ((-0.78) * ((dis_front_left / in->velocity) - 0.7));
    double dd = ((-0.78) * (((-dis_rear_left) / in->velocity) - 0.7));
    double p_left_lead = (1.0 - pow(con_e, cc));
    double p_left_lag = (1.0 - pow(con_e, dd));
    double p_left = (p_left_lead * p_left_lag);
    if (((dis_rear_left > (-size)) || (dis_front_left < size))) p_left = 0;
    double rnd1 = random_3(1.0);
    double rnd2 = random_3(1.0);
    if ((((rnd1 <= p_l) && (a_rear_l > (-2.0))) && (rnd2 <= p_left))) {
        if ((roads[in->pos.x].leftLaneId != (-1))) {
            out->pos.x = roads[in->pos.x].leftLaneId;
        }
    } else if ((((rnd1 <= (p_r + p_l)) && (a_rear_r > (-2.0))) && (rnd2 <= p_right))) {
        if ((roads[in->pos.x].rightLaneId != (-1))) {
            out->pos.x = roads[in->pos.x].rightLaneId;
        }
    } else {
        out->pos.x = in->pos.x;
    }
}

int main() {
    clock_t begin = clock();

    int lane = 1;
    int position = 4;
    for (int i = 0, _var0 = num_agents; i < _var0; ++i) {
        *DYN_ARRAY_PLACE(&agents.agents_Point, Point) = (Point) {
                .velocity = 1.0,
                .pos = float2_create(lane, position),
                .id = i,
                .isFinished = false,
        };
        position = position + 4;
        if (position > 200) {
            position = 0;
            lane++;
        }
    }
    for (int p = 1, _var1 = 2048; p < _var1; ++p) {
        if ((p < 2046)) {
            int next = (p + 3);
            int left = (((p - 1) > 0) ? (p - 1) : (-1));
            int right = (((p + 1) > 3) ? (-1) : (p + 1));
            Lane _var2 = (Lane) {
                    .laneId = p,
                    .length = 500.0,
                    .speedLimit = 30,
                    .nextLaneId = next,
                    .leftLaneId = left,
                    .rightLaneId = right,
            };
            Lane* lane = &_var2;
            roads[p] = *lane;
        } else {
            Lane _var3 = (Lane) {
                    .laneId = p,
                    .length = 500.0,
                    .speedLimit = 30,
                    .nextLaneId = (-1),
                    .leftLaneId = (-1),
                    .rightLaneId = (-1),
            };
            Lane* lane = &_var3;
            roads[p] = *lane;
        }
    }
    for (int _var19 = 0; _var19 < num_timesteps; _var19++) {
//        printf("iter %d\n",_var19);
        dyn_array tmp;
        if (!agents.agents_Point_dbuf.values) {
            agents.agents_Point_dbuf = DYN_ARRAY_COPY_FIXED(Point, &agents.agents_Point);
        }
//        #pragma omp parallel for
        for (size_t _var20 = 0; _var20 < agents.agents_Point.len; _var20++) {
            Point *_var21 = DYN_ARRAY_GET(&agents.agents_Point, Point, _var20);
            Point *_var22 = DYN_ARRAY_GET(&agents.agents_Point_dbuf, Point, _var20);
            car_follow(_var21, _var22);
        }
        tmp = agents.agents_Point;
        agents.agents_Point = agents.agents_Point_dbuf;
        agents.agents_Point_dbuf = tmp;
        if (!agents.agents_Point_dbuf.values) {
            agents.agents_Point_dbuf = DYN_ARRAY_COPY_FIXED(Point, &agents.agents_Point);
        }
//        #pragma omp parallel for
        for (size_t _var23 = 0; _var23 < agents.agents_Point.len; _var23++) {
            Point *_var24 = DYN_ARRAY_GET(&agents.agents_Point, Point, _var23);
            Point *_var25 = DYN_ARRAY_GET(&agents.agents_Point_dbuf, Point, _var23);
            lane_change(_var24, _var25);
        }
        tmp = agents.agents_Point;
        agents.agents_Point = agents.agents_Point_dbuf;
        agents.agents_Point_dbuf = tmp;
    }
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time elapsed is %f seconds\n", time_spent);
    return 0;
}
