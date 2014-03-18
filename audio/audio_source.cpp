/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#if !defined(FLOOR_NO_OPENAL)

#include "audio_source.hpp"
#include "audio_controller.hpp"

audio_source::audio_source(const string& identifier_,
						   const SOURCE_TYPE& type_,
						   weak_ptr<audio_store::audio_data> data_) noexcept :
identifier(identifier_), type(type_), data(data_) {
	audio_controller::acquire_context();
	
	AL(alGenSources(1, &source));
	
	auto data_ptr = data.lock();
	AL(alSourcei(source, AL_BUFFER, (ALint)data_ptr->buffer));
	
	set_volume(volume);
	set_position(position);
	if(type == SOURCE_TYPE::AUDIO_3D) {
		set_velocity(data_ptr->velocity);
		set_reference_distance(data_ptr->reference_distance);
		set_rolloff_factor(data_ptr->rolloff_factor);
		set_max_distance(data_ptr->max_distance);
		AL(alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE));
	}
	else { // SOURCE_TYPE::AUDIO_BACKGROUND
		set_velocity(velocity);
		set_reference_distance(reference_distance);
		set_rolloff_factor(rolloff_factor);
		set_max_distance(max_distance);
		AL(alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE));
		loop(true);
	}
	
	audio_controller::release_context();
}

audio_source::~audio_source() noexcept {
	if(alIsSource(source)) {
		alDeleteSources(1, &source);
	}
}

void audio_source::play() {
	audio_controller::acquire_context();
	AL(alSourcePlay(source));
	audio_controller::release_context();
	playing = true;
	paused = false;
}

void audio_source::pause() {
	audio_controller::acquire_context();
	AL(alSourcePause(source));
	audio_controller::release_context();
	playing = false;
	paused = true;
}

void audio_source::stop() {
	audio_controller::acquire_context();
	AL(alSourceStop(source));
	audio_controller::release_context();
	playing = false;
	paused = true;
}

void audio_source::rewind() {
	audio_controller::acquire_context();
	AL(alSourceRewind(source));
	audio_controller::release_context();
	playing = false;
	paused = true;
}

void audio_source::loop(const bool state) {
	audio_controller::acquire_context();
	AL(alSourcei(source, AL_LOOPING, (state ? AL_TRUE : AL_FALSE)));
	audio_controller::release_context();
	looping = state;
}

void audio_source::set_volume(const float& volume_) {
	audio_controller::acquire_context();
	AL(alSourcef(source, AL_GAIN, volume_));
	audio_controller::release_context();
	volume = volume_;
}

const float& audio_source::get_volume() const {
	return volume;
}

const string& audio_source::get_identifier() const {
	return identifier;
}

bool audio_source::is_playing() const {
	return playing;
}

bool audio_source::is_paused() const {
	return paused;
}

bool audio_source::is_looping() const {
	return looping;
}

ALint audio_source::update_source_state() {
	ALint source_state = 0;
	audio_controller::acquire_context();
	AL(alGetSourcei(source, AL_SOURCE_STATE, &source_state));
	audio_controller::release_context();
	
	switch(source_state) {
		case AL_INITIAL:
		case AL_PAUSED:
		case AL_STOPPED:
			playing = false;
			paused = true;
			break;
		case AL_PLAYING:
			playing = true;
			paused = false;
			break;
		default:
			floor_unreachable();
	}
	
	return source_state;
}

bool audio_source::query_playing() {
	update_source_state();
	return playing;
}

bool audio_source::query_paused() {
	update_source_state();
	return paused;
}

bool audio_source::query_looping() {
	ALint looping_state = 0;
	audio_controller::acquire_context();
	AL(alGetSourcei(source, AL_LOOPING, &looping_state));
	audio_controller::release_context();
	looping = (looping_state == AL_TRUE);
	return looping;
}

bool audio_source::query_initial() {
	return (update_source_state() == AL_INITIAL);
}

void audio_source::set_position(const float3& position_) {
	position = position_;
	audio_controller::acquire_context();
	AL(alSource3f(source, AL_POSITION, position.x, position.y, position.z));
	audio_controller::release_context();
}

const float3& audio_source::get_position() const {
	return position;
}

void audio_source::set_velocity(const float3& velocity_) {
	velocity = velocity_;
	audio_controller::acquire_context();
	AL(alSource3f(source, AL_VELOCITY, velocity.x, velocity.y, velocity.z));
	audio_controller::release_context();
}

const float3& audio_source::get_velocity() const {
	return velocity;
}

void audio_source::set_reference_distance(const float& reference_distance_) {
	reference_distance = reference_distance_;
	audio_controller::acquire_context();
	AL(alSourcef(source, AL_REFERENCE_DISTANCE, reference_distance));
	audio_controller::release_context();
}

const float& audio_source::get_reference_distance() const {
	return reference_distance;
}

void audio_source::set_rolloff_factor(const float& rolloff_factor_) {
	rolloff_factor = rolloff_factor_;
	audio_controller::acquire_context();
	AL(alSourcef(source, AL_ROLLOFF_FACTOR, rolloff_factor));
	audio_controller::release_context();
}

const float& audio_source::get_rolloff_factor() const {
	return rolloff_factor;
}

void audio_source::set_max_distance(const float& max_distance_) {
	max_distance = max_distance_;
	audio_controller::acquire_context();
	AL(alSourcef(source, AL_MAX_DISTANCE, max_distance));
	audio_controller::release_context();
}

const float& audio_source::get_max_distance() const {
	return max_distance;
}

#endif
