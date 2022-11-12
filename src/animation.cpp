#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include "game.hpp"
#include "animation.hpp"
#include "MediaManager.hpp"
#include <iostream>

AnimationFrame::AnimationFrame(int fn, const char *fpath, const char *spath)
{
	frame_path = fpath;
	sound = spath;
}

AnimationFrame::~AnimationFrame()
{
}

Animation::Animation(Game &game, int nframes, int l = 0) : game(game)
{
	if(l == 0)
		loops = false;
	else
		loops = true;

	current_frame = 0;
	start_tick = game.frame_counter.rendered_frames;
	animation_l = nframes;
}

Animation::Animation(const Animation &a) : game(a.game)
{
	current_frame = 0;
	start_tick = game.frame_counter.rendered_frames;
	frames = a.frames;
	animation_l = a.animation_l;
}

SDL_Texture* Animation::play()
{
	int frame_index = ((int)game.frame_counter.rendered_frames - start_tick) % animation_l;
	if(frames.find(frame_index) != frames.end()) {
		current_frame = frame_index;
		const char *p = frames[current_frame]->sound;
		
		if (strcmp(p, "NOSOUND")) {
			Mix_Chunk *s = game.media.readWAV(p);
			Mix_PlayChannel(-1, s, 0);
		}
	}

    SDL_Texture *tex = game.media.readTexture(frames[current_frame]->frame_path);

	return tex;
}


void Animation::set_frame(int fn, const char *fpath, const char *spath)
{
	frames.emplace(fn, std::make_shared<AnimationFrame>(fn, fpath, spath));
}

void Animation::set_frame_reactors(int fn, std::vector<ReactorCollideBox> r)
{
	frames[fn]->reactors = r;
}

void Animation::set_frame_activators(int fn, std::vector<ActivatorCollideBox> a)
{
	frames[fn]->activators = a;
}

SDL_Texture *Animation::get_frame() {
	SDL_Texture *texture = game.media.readTexture(frames[current_frame]->frame_path);
	return texture;
}

std::vector<ReactorCollideBox> Animation::frame_reactors() {
	int frame_index = ((int)game.frame_counter.rendered_frames - start_tick) % animation_l;
	if(frames.find(frame_index) != frames.end()) {
		return frames[frame_index]->reactors;	
	}

	std::vector<ReactorCollideBox> empty;
	return empty;
}

std::vector<ActivatorCollideBox> Animation::frame_activators() {
	int frame_index = ((int)game.frame_counter.rendered_frames - start_tick) % animation_l;
	if(frames.find(current_frame) != frames.end()) {
		//std::cout << "returning " << frames[current_frame]->activators.size() << " for frame index " << current_frame << std::endl;
		return frames[current_frame]->activators;	
	}
 
	std::vector<ActivatorCollideBox> empty;
	return empty;
}

void Animation::add_activator(int fn, ActivatorCollideBox box) {
	frames[fn]->activators.push_back(box);
}

void Animation::add_reactor(int fn, ReactorCollideBox box) {
	frames[fn]->reactors.push_back(box);
}

void Animation::reset() {
	current_frame = 0;
	start_tick = game.frame_counter.rendered_frames;
}

bool Animation::is_over() {
	if (!loops)
		return (int)game.frame_counter.rendered_frames - start_tick >= animation_l;
	return false;
}
