// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2022-2023 by Ronald "Eidolon" Kinard
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "music_player.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <gme/gme.h>
#include <stb_vorbis.h>
#undef byte	 // BLARGG!! NO!!
#undef check // STOP IT!!!!

#include "../cxxutil.hpp"
#include "../io/streams.hpp"
#include "gme_player.hpp"
#include "ogg_player.hpp"
#include "resample.hpp"
#include "xmp_player.hpp"

using std::array;
using std::byte;
using std::make_unique;
using std::size_t;
using std::vector;

using srb2::audio::MusicPlayer;
using srb2::audio::Resampler;
using srb2::audio::Sample;
using srb2::audio::Source;
using namespace srb2;

class MusicPlayer::Impl {
public:
	Impl() = default;
	Impl(tcb::span<std::byte> data) : Impl() { _load(data); }

	size_t generate(tcb::span<Sample<2>> buffer) {
		if (!resampler_)
			return 0;

		if (!playing_)
			return 0;

		size_t total_written = 0;

		while (total_written < buffer.size()) {
			const size_t generated = resampler_->generate(buffer.subspan(total_written));

			// To avoid a branch preventing optimizations, we're always going to apply
			// the fade gain, even if it would clamp anyway.
			for (std::size_t i = 0; i < generated; i++) {
				const float alpha = 1.0 - (gain_samples_target_ - std::min(gain_samples_ + i, gain_samples_target_)) /
											  static_cast<double>(gain_samples_target_);
				const float fade_gain = (gain_target_ - gain_) * std::clamp(alpha, 0.f, 1.f) + gain_;
				buffer[total_written + i] *= fade_gain;
			}

			gain_samples_ = std::min(gain_samples_ + generated, gain_samples_target_);

			if (gain_samples_ >= gain_samples_target_) {
				fading_ = false;
				gain_samples_ = gain_samples_target_;
				gain_ = gain_target_;
			}

			total_written += generated;

			if (generated == 0) {
				playing_ = false;
				break;
			}
		}

		return total_written;
	}

	void _load(tcb::span<std::byte> data) {
		ogg_inst_ = nullptr;
		gme_inst_ = nullptr;
		xmp_inst_ = nullptr;
		resampler_ = std::nullopt;

		try {
			io::SpanStream stream {data};
			audio::Ogg ogg = audio::load_ogg(stream);
			ogg_inst_ = std::make_shared<audio::OggPlayer<2>>(std::move(ogg));
			ogg_inst_->looping(looping_);
			resampler_ = Resampler<2>(ogg_inst_, ogg_inst_->sample_rate() / 44100.f);
		} catch (const std::exception& ex) {
			// it's probably not ogg
			ogg_inst_ = nullptr;
			resampler_ = std::nullopt;
		}

		if (!resampler_) {
			try {
				if (data[0] == std::byte {0x1F} && data[1] == std::byte {0x8B}) {
					io::ZlibInputStream stream {io::SpanStream(data)};
					audio::Gme gme = audio::load_gme(stream);
					gme_inst_ = std::make_shared<GmePlayer<2>>(std::move(gme));
				} else {
					io::SpanStream stream {data};
					audio::Gme gme = audio::load_gme(stream);
					gme_inst_ = std::make_shared<GmePlayer<2>>(std::move(gme));
				}

				resampler_ = Resampler<2>(gme_inst_, 1.f);
			} catch (const std::exception& ex) {
				// it's probably not gme
				gme_inst_ = nullptr;
				resampler_ = std::nullopt;
			}
		}

		if (!resampler_) {
			try {
				io::SpanStream stream {data};
				audio::Xmp<2> xmp = audio::load_xmp<2>(stream);
				xmp_inst_ = std::make_shared<XmpPlayer<2>>(std::move(xmp));
				xmp_inst_->looping(looping_);

				resampler_ = Resampler<2>(xmp_inst_, 1.f);
			} catch (const std::exception& ex) {
				// it's probably not xmp
				xmp_inst_ = nullptr;
				resampler_ = std::nullopt;
			}
		}

		playing_ = false;

		internal_gain(1.f);
	}

	void play(bool looping) {
		if (ogg_inst_) {
			ogg_inst_->looping(looping);
			ogg_inst_->playing(true);
			playing_ = true;
			ogg_inst_->reset();
		} else if (gme_inst_) {
			playing_ = true;
			gme_inst_->reset();
		} else if (xmp_inst_) {
			xmp_inst_->looping(looping);
			playing_ = true;
			xmp_inst_->reset();
		}
	}

	void unpause() {
		if (ogg_inst_) {
			ogg_inst_->playing(true);
			playing_ = true;
		} else if (gme_inst_) {
			playing_ = true;
		} else if (xmp_inst_) {
			playing_ = true;
		}
	}

	void pause() {
		if (ogg_inst_) {
			ogg_inst_->playing(false);
			playing_ = false;
		} else if (gme_inst_) {
			playing_ = false;
		} else if (xmp_inst_) {
			playing_ = false;
		}
	}

	void stop() {
		if (ogg_inst_) {
			ogg_inst_->reset();
			ogg_inst_->playing(false);
			playing_ = false;
		} else if (gme_inst_) {
			gme_inst_->reset();
			playing_ = false;
		} else if (xmp_inst_) {
			xmp_inst_->reset();
			playing_ = false;
		}
	}

	void seek(float position_seconds) {
		if (ogg_inst_) {
			ogg_inst_->seek(position_seconds);
			return;
		}
		if (gme_inst_) {
			gme_inst_->seek(position_seconds);
			return;
		}
		if (xmp_inst_) {
			xmp_inst_->seek(position_seconds);
			return;
		}
	}

	bool playing() const {
		if (ogg_inst_)
			return ogg_inst_->playing();
		else if (gme_inst_)
			return playing_;
		else if (xmp_inst_)
			return playing_;

		return false;
	}

	std::optional<audio::MusicType> music_type() const {
		if (ogg_inst_)
			return audio::MusicType::kOgg;
		else if (gme_inst_)
			return audio::MusicType::kGme;
		else if (xmp_inst_)
			return audio::MusicType::kMod;

		return std::nullopt;
	}

	std::optional<float> duration_seconds() const {
		if (ogg_inst_)
			return ogg_inst_->duration_seconds();
		if (gme_inst_)
			return gme_inst_->duration_seconds();
		if (xmp_inst_)
			return xmp_inst_->duration_seconds();

		return std::nullopt;
	}

	std::optional<float> loop_point_seconds() const {
		if (ogg_inst_)
			return ogg_inst_->loop_point_seconds();
		if (gme_inst_)
			return gme_inst_->loop_point_seconds();

		return std::nullopt;
	}

	std::optional<float> position_seconds() const {
		if (ogg_inst_)
			return ogg_inst_->position_seconds();
		if (gme_inst_)
			return gme_inst_->position_seconds();

		return std::nullopt;
	}

	void fade_to(float gain, float seconds) { fade_from_to(gain_target_, gain, seconds); }

	void fade_from_to(float from, float to, float seconds) {
		fading_ = true;
		gain_ = from;
		gain_target_ = to;
		// Gain samples target must always be at least 1 to avoid a div-by-zero.
		gain_samples_target_ = std::max(static_cast<uint64_t>(seconds * 44100.f), 1ULL);
		gain_samples_ = 0;
	}

	bool fading() const { return fading_; }

	void stop_fade() { internal_gain(gain_target_); }

	void loop_point_seconds(float loop_point) {
		if (ogg_inst_)
			ogg_inst_->loop_point_seconds(loop_point);
	}

	void internal_gain(float gain) {
		fading_ = false;
		gain_ = gain;
		gain_target_ = gain;
		gain_samples_target_ = 1;
		gain_samples_ = 0;
	}

private:
	std::shared_ptr<OggPlayer<2>> ogg_inst_;
	std::shared_ptr<GmePlayer<2>> gme_inst_;
	std::shared_ptr<XmpPlayer<2>> xmp_inst_;
	std::optional<Resampler<2>> resampler_;
	bool playing_ {false};
	bool looping_ {false};

	// fade control
	float gain_target_ {1.f};
	float gain_ {1.f};
	bool fading_ {false};
	uint64_t gain_samples_ {0};
	uint64_t gain_samples_target_ {1};
};

// The special member functions MUST be declared in this unit, where Impl is complete.
MusicPlayer::MusicPlayer() : impl_(make_unique<MusicPlayer::Impl>()) {
}
MusicPlayer::MusicPlayer(tcb::span<std::byte> data) : impl_(make_unique<MusicPlayer::Impl>(data)) {
}
MusicPlayer::MusicPlayer(MusicPlayer&& rhs) noexcept = default;
MusicPlayer& MusicPlayer::operator=(MusicPlayer&& rhs) noexcept = default;

MusicPlayer::~MusicPlayer() = default;

void MusicPlayer::play(bool looping) {
	SRB2_ASSERT(impl_ != nullptr);

	return impl_->play(looping);
}

void MusicPlayer::unpause() {
	SRB2_ASSERT(impl_ != nullptr);

	return impl_->unpause();
}

void MusicPlayer::pause() {
	SRB2_ASSERT(impl_ != nullptr);

	return impl_->pause();
}

void MusicPlayer::stop() {
	SRB2_ASSERT(impl_ != nullptr);

	return impl_->stop();
}

void MusicPlayer::seek(float position_seconds) {
	SRB2_ASSERT(impl_ != nullptr);

	return impl_->seek(position_seconds);
}

bool MusicPlayer::playing() const {
	SRB2_ASSERT(impl_ != nullptr);

	return impl_->playing();
}

size_t MusicPlayer::generate(tcb::span<Sample<2>> buffer) {
	SRB2_ASSERT(impl_ != nullptr);

	return impl_->generate(buffer);
}

std::optional<audio::MusicType> MusicPlayer::music_type() const {
	SRB2_ASSERT(impl_ != nullptr);

	return impl_->music_type();
}

std::optional<float> MusicPlayer::duration_seconds() const {
	SRB2_ASSERT(impl_ != nullptr);

	return impl_->duration_seconds();
}

std::optional<float> MusicPlayer::loop_point_seconds() const {
	SRB2_ASSERT(impl_ != nullptr);

	return impl_->loop_point_seconds();
}

std::optional<float> MusicPlayer::position_seconds() const {
	SRB2_ASSERT(impl_ != nullptr);

	return impl_->position_seconds();
}

void MusicPlayer::fade_to(float gain, float seconds) {
	SRB2_ASSERT(impl_ != nullptr);

	impl_->fade_to(gain, seconds);
}

void MusicPlayer::fade_from_to(float from, float to, float seconds) {
	SRB2_ASSERT(impl_ != nullptr);

	impl_->fade_from_to(from, to, seconds);
}

void MusicPlayer::internal_gain(float gain) {
	SRB2_ASSERT(impl_ != nullptr);

	impl_->internal_gain(gain);
}

bool MusicPlayer::fading() const {
	SRB2_ASSERT(impl_ != nullptr);

	return impl_->fading();
}

void MusicPlayer::stop_fade() {
	SRB2_ASSERT(impl_ != nullptr);

	impl_->stop_fade();
}

void MusicPlayer::loop_point_seconds(float loop_point) {
	SRB2_ASSERT(impl_ != nullptr);

	impl_->loop_point_seconds(loop_point);
}
