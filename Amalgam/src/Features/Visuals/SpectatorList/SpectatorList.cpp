#include "SpectatorList.h"

static inline float ClampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float MinF(float a, float b) { return a < b ? a : b; }
static inline float LerpF(float a, float b, float t) { return a + (b - a) * ClampF(t, 0.f, 1.f); }

// fade towards background color (works with Color_t.Lerp, no ImColor needed)
static inline Color_t ModAlpha(Color_t c, float a)
{
	return c.Lerp(Vars::Menu::Theme::Background.Value, 1.f - ClampF(a, 0.f, 1.f));
}

static inline int FloorI(float v)
{
	int i = (int)v;
	return v < (float)i ? i - 1 : i;
}

bool CSpectatorList::IsF2P(int iIndex)
{
	for (const auto& tPlayer : F::PlayerUtils.m_vPlayerCache)
	{
		if (I::EngineClient->GetPlayerForUserID(tPlayer.m_iUserID) == iIndex)
			return tPlayer.m_bF2P;
	}
	return false;
}

Color_t CSpectatorList::GetRowColor(const Spectator_t& t)
{
	Color_t c = Vars::Menu::Theme::Active.Value;
	if (t.m_bFriend)
		c = F::PlayerUtils.m_vTags[F::PlayerUtils.TagToIndex(FRIEND_TAG)].m_tColor;
	else if (t.m_bParty)
		c = F::PlayerUtils.m_vTags[F::PlayerUtils.TagToIndex(PARTY_TAG)].m_tColor;
	else if (t.m_bRespawnTimeIncreased)
		c = F::PlayerUtils.m_vTags[F::PlayerUtils.TagToIndex(CHEATER_TAG)].m_tColor;
	else if (t.m_bF2P)
		c = F::PlayerUtils.m_vTags[F::PlayerUtils.TagToIndex(F2P_TAG)].m_tColor;
	else if (FNV1A::Hash32(t.m_sMode) == FNV1A::Hash32Const("1st"))
		c = c.Lerp({ 255, 150, 0, 255 }, 0.5f);
	return c;
}

bool CSpectatorList::GetSpectators(CTFPlayer* pTarget)
{
	m_vSpectators.clear();

	auto pResource = H::Entities.GetResource();
	if (!pResource || !pTarget)
		return false;

	const int iLocal = I::EngineClient->GetLocalPlayer();
	const int iTarget = pTarget->entindex();

	for (int n = 1; n <= I::EngineClient->GetMaxClients(); n++)
	{
		auto pPlayer = I::ClientEntityList->GetClientEntity(n)->As<CTFPlayer>();
		const bool bLocal = n == iLocal;
		const bool bF2P = IsF2P(n);
		const bool bFriend = H::Entities.IsFriend(n);
		const bool bParty = H::Entities.InParty(n);

		if (pResource->m_bValid(n) && !pResource->IsFakePlayer(n)
			&& pResource->m_iTeam(iLocal) != TEAM_SPECTATOR && pResource->m_iTeam(n) == TEAM_SPECTATOR)
		{
			m_vSpectators.emplace_back(
				F::PlayerUtils.GetPlayerName(n, pResource->GetName(n)),
				"possible", -1.f, false, n, bF2P, bFriend, bParty);
			continue;
		}

		if (iTarget == n || pResource->IsFakePlayer(n)
			|| !pPlayer || !pPlayer->IsPlayer() || pPlayer->IsAlive()
			|| pTarget->IsDormant() != pPlayer->IsDormant()
			|| pResource->m_iTeam(iTarget) != pResource->m_iTeam(n))
		{
			if (m_mRespawnCache.contains(n))
				m_mRespawnCache.erase(n);
			continue;
		}

		int iObserverTarget = !pPlayer->IsDormant() ? pPlayer->m_hObserverTarget().GetEntryIndex() : iTarget;
		int iObserverMode = pPlayer->m_iObserverMode();
		if (bLocal && F::Spectate.HasTarget())
		{
			iObserverTarget = F::Spectate.m_hOriginalTarget.GetEntryIndex();
			iObserverMode = F::Spectate.m_iOriginalMode;
		}
		if (iObserverTarget != iTarget || bLocal && !I::EngineClient->IsPlayingDemo() && !F::Spectate.HasTarget())
		{
			if (m_mRespawnCache.contains(n))
				m_mRespawnCache.erase(n);
			continue;
		}

		const char* sMode = "possible";
		if (!pPlayer->IsDormant())
		{
			switch (iObserverMode)
			{
			case OBS_MODE_FIRSTPERSON: sMode = "1st"; break;
			case OBS_MODE_THIRDPERSON: sMode = "3rd"; break;
			default: continue;
			}
		}

		float flRespawnTime = 0.f, flRespawnIn = -1.f;
		bool bRespawnTimeIncreased = false;
		if (pPlayer->IsInValidTeam())
		{
			flRespawnTime = pResource->m_flNextRespawnTime(n);
			const int iFloor = FloorI(flRespawnTime - TICKS_TO_TIME(I::ClientState->m_ClockDriftMgr.m_nServerTick));
			flRespawnIn = iFloor < 0 ? 0.f : (float)iFloor;
			if (!m_mRespawnCache.contains(n))
				m_mRespawnCache[n] = flRespawnTime;
			else if (m_mRespawnCache[n] + 0.5f < flRespawnTime)
				bRespawnTimeIncreased = true;
		}

		m_vSpectators.emplace_back(
			F::PlayerUtils.GetPlayerName(n, pResource->GetName(n)),
			sMode, flRespawnIn, bRespawnTimeIncreased, n, bF2P, bFriend, bParty);
	}

	// insertion sort — friend > party > alphabetical
	for (size_t i = 1; i < m_vSpectators.size(); i++)
	{
		Spectator_t tKey = m_vSpectators[i];
		size_t j = i;
		while (j > 0)
		{
			const Spectator_t& tPrev = m_vSpectators[j - 1];
			bool bPrevFirst;
			if (tPrev.m_bFriend != tKey.m_bFriend)
				bPrevFirst = tPrev.m_bFriend;
			else if (tPrev.m_bParty != tKey.m_bParty)
				bPrevFirst = tPrev.m_bParty;
			else
				bPrevFirst = tPrev.m_sName <= tKey.m_sName;
			if (bPrevFirst)
				break;
			m_vSpectators[j] = m_vSpectators[j - 1];
			j--;
		}
		m_vSpectators[j] = tKey;
	}

	return !m_vSpectators.empty();
}

void CSpectatorList::Draw(CTFPlayer* pLocal)
{
	if (!(Vars::Menu::Indicators.Value & Vars::Menu::IndicatorsEnum::Spectators))
	{
		m_mRespawnCache.clear();
		m_mAnim.clear();
		m_vSnapshot.clear();
		m_vSpectators.clear();
		m_flPanelAlpha = 0.f;
		m_flPanelSlide = 0.f;
		m_flLastTime = -1.f;
		m_flCountPulse = 0.f;
		m_iLastCount = 0;
		return;
	}
	if (!pLocal)
		return;

	const float flTime = I::GlobalVars->curtime;
	const float flDelta = m_flLastTime < 0.f ? 0.f : ClampF(flTime - m_flLastTime, 0.f, 0.1f);
	m_flLastTime = flTime;

	auto pTarget = pLocal;
	switch (pLocal->m_iObserverMode())
	{
	case OBS_MODE_FIRSTPERSON:
	case OBS_MODE_THIRDPERSON:
		pTarget = pLocal->m_hObserverTarget()->As<CTFPlayer>();
	}

	const bool bHasTarget = pTarget && pTarget->IsPlayer();
	const bool bHas = bHasTarget && GetSpectators(pTarget);
	const float flWasAlpha = m_flPanelAlpha;

	if (bHas)
	{
		m_vSnapshot = m_vSpectators;
		if (flWasAlpha < 0.05f)
			m_flPanelSlide = -10.f;
		m_flPanelAlpha = LerpF(m_flPanelAlpha, 1.f, flDelta * 12.f);
		m_flPanelSlide = LerpF(m_flPanelSlide, 0.f, flDelta * 14.f);
		if ((int)m_vSpectators.size() != m_iLastCount)
		{
			m_flCountPulse = 1.f;
			m_iLastCount = (int)m_vSpectators.size();
		}
	}
	else if (m_flPanelAlpha > 0.001f)
	{
		m_vSpectators = m_vSnapshot;
		m_flPanelAlpha = ClampF(m_flPanelAlpha - flDelta * 5.f, 0.f, 1.f);
		m_flPanelSlide = LerpF(m_flPanelSlide, 4.f, flDelta * 8.f);
	}
	else
	{
		m_vSnapshot.clear();
		m_mAnim.clear();
		m_vSpectators.clear();
		m_iLastCount = 0;
		m_flCountPulse = 0.f;
		return;
	}

	m_flCountPulse = ClampF(m_flCountPulse - flDelta * 1.6f, 0.f, 1.f);

	for (auto& tSpec : m_vSpectators)
	{
		auto& tAnim = m_mAnim[tSpec.m_iIndex];
		if (!tAnim.m_bInit)
		{
			tAnim.m_bInit = true;
			tAnim.m_flFirstSeen = flTime;
			tAnim.m_flPulse = 1.f;
			tAnim.m_tColorCur = GetRowColor(tSpec);
		}
		else if (flTime - tAnim.m_flLastSeen > 0.3f && tAnim.m_flFirstSeen < flTime - 0.1f)
		{
			tAnim.m_flFirstSeen = flTime;
			tAnim.m_flPulse = 1.f;
		}
		tAnim.m_flLastSeen = flTime;
	}
	for (auto it = m_mAnim.begin(); it != m_mAnim.end(); )
	{
		if (flTime - it->second.m_flLastSeen > 2.f)
			it = m_mAnim.erase(it);
		else
			++it;
	}

	int x = Vars::Menu::SpectatorsDisplay.Value.x;
	const int yBase = Vars::Menu::SpectatorsDisplay.Value.y + 8 + (int)m_flPanelSlide;
	const auto& fFont = H::Fonts.GetFont(FONT_INDICATORS);
	const int nTall = fFont.m_nTall + H::Draw.Scale(3);

	EAlign align = ALIGN_TOP;
	if (x <= 100 + H::Draw.Scale(50, Scale_Round))
	{
		x -= H::Draw.Scale(42, Scale_Round);
		align = ALIGN_TOPLEFT;
	}
	else if (x >= H::Draw.m_nScreenW - 100 + H::Draw.Scale(50, Scale_Round))
	{
		x += H::Draw.Scale(42, Scale_Round);
		align = ALIGN_TOPRIGHT;
	}

	const char* sName = "You";
	if (pTarget && pTarget != pLocal)
	{
		if (auto pResource = H::Entities.GetResource())
			sName = F::PlayerUtils.GetPlayerName(pTarget->entindex(), pResource->GetName(pTarget->entindex()));
	}

	{
		Color_t tHeader = Vars::Menu::Theme::Accent.Value;
		if (m_flCountPulse > 0.f)
			tHeader = tHeader.Lerp({ 255, 255, 255, 255 }, m_flCountPulse * m_flCountPulse * 0.6f);
		tHeader = ModAlpha(tHeader, m_flPanelAlpha);

		H::Draw.StringOutlined(fFont, x, yBase, tHeader, Vars::Menu::Theme::Background.Value, align,
			std::format("Spectating {} ({}):", sName, m_vSpectators.size()).c_str());
	}

	int y = yBase;
	int iRowIdx = 0;
	for (auto& tSpec : m_vSpectators)
	{
		y += nTall;
		auto& tAnim = m_mAnim[tSpec.m_iIndex];

		const float flStagger = iRowIdx * 0.05f;
		const float flAge = flTime - tAnim.m_flFirstSeen - flStagger;
		tAnim.m_flAlpha = ClampF(flAge / 0.25f, 0.f, 1.f);
		tAnim.m_flPulse = ClampF(tAnim.m_flPulse - flDelta * 1.3f, 0.f, 1.f);

		const Color_t tTarget = GetRowColor(tSpec);
		tAnim.m_tColorCur = tAnim.m_tColorCur.Lerp(tTarget, MinF(flDelta * 10.f, 1.f));

		Color_t tDraw = tAnim.m_tColorCur;

		if (tAnim.m_flPulse > 0.f)
		{
			const float flEase = tAnim.m_flPulse * tAnim.m_flPulse;
			tDraw = tDraw.Lerp({ 255, 255, 255, 255 }, flEase * 0.75f);
		}

		if (tSpec.m_flRespawnIn >= 0.f && tSpec.m_flRespawnIn <= 5.f)
		{
			const float flPhase = flTime * 7.f;
			const float flFrac = flPhase - (float)FloorI(flPhase);
			const float flBlink = flFrac < 0.5f ? flFrac * 2.f : (1.f - flFrac) * 2.f;
			tDraw = tDraw.Lerp({ 255, 90, 90, 255 }, flBlink * 0.55f);
		}

		tDraw = ModAlpha(tDraw, tAnim.m_flAlpha * m_flPanelAlpha);

		const char* sAccount = tSpec.m_bF2P ? "[F2P]" : "[P2P]";

		if (tSpec.m_flRespawnIn != -1.f)
			H::Draw.StringOutlined(fFont, x, y, tDraw, Vars::Menu::Theme::Background.Value, align,
				std::format("{} {} ({} - respawn {}s)", sAccount, tSpec.m_sName, tSpec.m_sMode, tSpec.m_flRespawnIn).c_str());
		else
			H::Draw.StringOutlined(fFont, x, y, tDraw, Vars::Menu::Theme::Background.Value, align,
				std::format("{} {} ({})", sAccount, tSpec.m_sName, tSpec.m_sMode).c_str());

		iRowIdx++;
	}
}
