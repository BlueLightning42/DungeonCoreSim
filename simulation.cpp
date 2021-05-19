#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
#include <limits>
#include <tuple>
#include <fstream>
#include <chrono>

// if using c++20 its defined in the std but when I compiled it online I only had c++17
constexpr double pi = 3.14159265358979323846;

struct Core{
    int level;
    double days;
    int born;
    // flags
    bool to_die;
    bool to_advance;
};

// default stats for each level- prevent some calculations from being done multiple times.
class Level{
 public:
    Level(){};
    Level(int lvl){
        exp = lvl_to_exp(lvl);
        rank = exp_to_rank(exp);
        death_chance = death_percent(lvl, rank);
        average_days = exp_to_days(exp);
    }
    int exp;
    int rank;
    double death_chance;
    double average_days;
    int died=0;
    int passed=0;
 private:
    // experiance required for each level is parabalic...
    int lvl_to_exp(int lvl);
    int exp_to_rank(int exp);
    double exp_to_days(int exp);
    double death_percent(int level, int rank);
};
// experiance required for each level is parabalic...
int Level::lvl_to_exp(int lvl){
    return lvl*lvl + 11*lvl + 10;
}
// total exp is roughtly the volume of the core.
// turning volume of core into a size in radius than log2+ceil'ing it so rank 2 is radius 2 rank 3 is radius 4, rank 4 radius 8 etc.
int Level::exp_to_rank(int exp){
    double tmp = std::pow(exp/(pi*4/3.), 1/3.);
    tmp /= 1.73824;
    tmp = std::log2(tmp);
    return (int)std::ceil(tmp);
}
// as level/required exp increases days increases but not linearly as its easier to gain exp in higher levels.
double Level::exp_to_days(int exp){
    return std::pow(exp, 4./5.)*0.1;
}
/* closer you reach to rank 3/breaking the surface 
* the higher and higher the % chance of dying/getting stuck.
* afterwards theres a smaller correlation to higher levels 
* being smarter/less likely to die on a per day basis. 
* (but due to time increasing per level it doesn't stack upwards as much)
* this calculation is mostly to prevent infinite cores 
* at the highest levels due to dungeon cores not aging 
* and only dying from getting smashed while letting me spawn
* enough low level cores a day for there to be other cores
* at rank 1/the first few weeks to talk to.
*/
double Level::death_percent(int level, int rank){
    if (rank < 3){
        return level / 20000.0;
    }if (rank > 5){
        return 0; // gods become truely imortal
    }
    return 0.00009/sqrt(level) -0.000001;  //Somewhat randomly chosen...kept changing it unstil I got the distribution I wanted
}

// main program
class Population{
 public:
/* when the system came x years ago it was given to
* initial_count cores. These cores are the Ancients
* some of the only creatures still alive that were before the system.
* rather than match the spread that it settles at 
* "after x years". I'm spawning a ainchent per level * 4-rank.
* in the hopes that it works itself out nicely 
* and theres only a few left at the end of the simulation.
* also not giving them different random times to simulate them all getting a mini tutorial to advance one level.
*/
    Population(): gen(std::random_device{}()), 
                ldist(0, 1.0), udist(0.0, 1.1775) {
        const auto initial_count = 100; //NOTE IF HIGHER MAKE 4-rank higher
        defaults_for_levels.reserve(initial_count);
        cores.reserve(initial_count);
        Core core_template = {
            0,
            1.0,
            std::numeric_limits<int>::lowest(),
            false,
            false
        };
        for(int i=0; i<initial_count; i++){
            defaults_for_levels.emplace_back(i);
            core_template.level = i;
            cores.emplace_back();
            auto number_to_spawn = 4 - defaults_for_levels.back().rank;
            for(int _=0; _< number_to_spawn; _++){
                cores.back().push_back(core_template);
            }
        }
    };
    void run_to(int year);
 private:
    std::vector<Level> defaults_for_levels;
    std::vector<std::vector<Core>> cores;
    std::mt19937 gen;
    std::lognormal_distribution<> ldist;
    std::uniform_real_distribution<> udist;
    double calculate_days(int level);
    void step_simulation(int year);
    void display_stats(int year);
};

// random days it takes a core to advance. Harder with higher levels
// base is multiplied by ldist which is centered roughly around 1 but has a few much higher outliers to simulate getting "stuck" for much longer at a level...some cores reach a level and suddenly can't progress.

double Population::calculate_days(int level){
    double days = defaults_for_levels.at(level).average_days;
    double random_extra = ldist(gen) /2.0 + 0.5; // Think I centered it on 1.0 so the mean is 1.0 and the distribution is 0.5-X
    return days * random_extra;
}
// simulation steps forward to x years. (provides visual queue its doing something every 5000 years by showing how much time those 5000 years took to simulate...as sim progresses more cores are tracked so time increases)
void Population::run_to(int year = 100){
    using namespace std::chrono;
    auto start = steady_clock::now();
    for(int i=0; i<year; i++){
        step_simulation(i);
        if ( i % (365*100) == 0){
            auto end = steady_clock::now();
            auto elapsed = duration_cast<seconds>(end-start);
            std::cout 
                << "Year '" << i / 365
                << "': time for simulation " 
                << elapsed.count() << "s\n";
            start = end;
        }
    }
    display_stats(year);
}
// step a year of the simulation.
void Population::step_simulation(int year){
    // 40% chance a core spawns each day.
    if( udist(gen) < 0.4){
        // emplace not working after I added the flags? quick fix to this
        cores.at(0).push_back({0, 1.0, year, false, false});
    }
    for(int level = 0; level < cores.size(); level++){
        if (cores.at(level).size() <= 0 ) continue;
        for( auto & core : cores.at(level)){
            bool survived = udist(gen) > 
                defaults_for_levels.at(level).death_chance;
            if(survived){
                core.days -= 1.0;
                if ( core.days < 1.0 ){
                   core.to_advance = true;
                }
            }else{
                core.to_die = true;
                defaults_for_levels.at(level).died += 1;
            }
        }
    }
    const int current_max = cores.size(); // core size increases if any core advances.
    for(int level = 0; level < current_max; level++){
        if (cores.at(level).size() <= 0 ) continue;
        std::vector<Core> advancing;
        for(const auto & core: cores.at(level)){
            if (core.to_advance){
                if(defaults_for_levels.size() <= level + 1){
                    defaults_for_levels.emplace_back(level + 1);
                }
                defaults_for_levels.at(level + 1).passed += 1;
                double days = calculate_days(level+1);
                auto core_copy = core;
                core_copy.days += days;
                core_copy.to_advance = false;
                advancing.push_back(core_copy);
            }
        }
        auto to_remove = std::remove_if(
            cores.at(level).begin(), 
            cores.at(level).end(), 
            [](const Core& core)
            { return core.to_die || core.to_advance; }
        );
        cores.at(level).erase(to_remove, cores.at(level).end());
        if(advancing.size() <= 0) continue;
        
        if ( level + 1 >= cores.size()){
            cores.push_back({});
        }
        cores.at(level+1).insert(cores.at(level+1).end(), advancing.begin(), advancing.end());
    }
}
// does nothing but write to csv and stdout and add up some averages.
void Population::display_stats(int year){
    std::vector<std::tuple<int,int,int>> ranks;
    for(int i=0; i < 7; i++){
        ranks.emplace_back(0,0,0);
    }
    std::ofstream file ("output.csv");
    file << "level, rank, passed, died, final, average_age, number of Ancients\n";
    int total_ancient_count = 0;
    for(int i=1; i < cores.size(); i++){
        auto &[p,d,f] = ranks.at(defaults_for_levels.at(i).rank);
        const auto pa = defaults_for_levels.at(i).passed;
        const auto da = defaults_for_levels.at(i).died;
        const auto fa = cores.at(i).size();
        const auto rank = defaults_for_levels.at(i).rank;
        if( defaults_for_levels.at(i - 1).rank != rank){
            p += pa;
        }
        d += da;

        int average_age = 0;
        int new_cores = 0;
        int ancient_count = 0;
        for(const auto & core : cores.at(i)){
            if (core.born >= 0){
                average_age += year - core.born;
                new_cores++;
            }else{
                total_ancient_count++;
                ancient_count++;
            }
        }
        if(new_cores > 0){
            average_age /= new_cores;
        }else{
            average_age = -1;
        }

        file << i << "," << rank << "," << pa << "," << da << "," << fa << ",";
        if (average_age > 0) file << average_age; // blanks for ones I can't calculate
        file << "," << ancient_count << "\n";

        if(cores.at(i).size() <= 0) continue;
        f += fa;
        std::cout 
            << "LVL: " << i << "  \tRank: " << rank
            << "\n\tpassed:" << pa
            << "\n\tdied:" << da 
            << "\n\t%died:" << da*100.0/pa
            << "\n\taverage_age:" << average_age 
            << " days | Year " << average_age / 365
            << "\n\tancients alive:" << ancient_count
            << "\n\tfinal:" << fa << "\n";
    }
    file.close();
    std::cout << "\nTotal ancient's still alive " << total_ancient_count;
    for(int i=1; i < 7; i++){
        const auto &[p,d,f] = ranks.at(i);
        if( p == 0) break;
        std::cout 
            << "\nRank: " << i
            << "\n\tpassed:" << p
            << "\n\tdied:" << d
            << "\n\tfinal:" << f 
            << "\n\t%died:" << d*100.0/p;
    }
}

int main() {
    // Less a full program this is kinda just a single use single page calculation.
    Population pop;
    pop.run_to(365*1800);
}
