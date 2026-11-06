#pragma once
#include "../../../SDK/SDK.h"
#include "../../Players/PlayerUtils.h"
#include "../../Spectate/Spectate.h"

class CSpectatorList
{
private:
	struct Spectator_t
	{
		std::string m_sName;
		const char* m_sMode;
		float m_flRespawnIn;
		bool m_bRespawnTimeIncreased;
		int m_iIndex;
		bool m_bF2P;
		bool m_bFriend;
		bool m_bParty;
	};

	struct RowAnim_t
	{
		float m_flAlpha = 0.f;
		float m_flPulse = 0.f;
		float m_flFirstSeen = 0.f;
		float m_flLastSeen = 0.f;
		Color_t m_tColorCur = { 255, 255, 255, 255 };
		bool m_bInit = false;
	};

	std::vector<Spectator_t> m_vSpectators = {};
	std::vector<Spectator_t> m_vSnapshot = {};
	std::unordered_map<int, float> m_mRespawnCache = {};
	std::unordered_map<int, RowAnim_t> m_mAnim = {};

	float m_flPanelAlpha = 0.f;
	float m_flPanelSlide = 0.f;
	float m_flLastTime = -1.f;
	float m_flCountPulse = 0.f;
	int m_iLastCount = 0;

	bool IsF2P(int iIndex);
	Color_t GetRowColor(const Spectator_t& t);

public:
	bool GetSpectators(CTFPlayer* pTarget);
	void Draw(CTFPlayer* pLocal);
};

ADD_FEATURE(CSpectatorList, SpectatorList);
