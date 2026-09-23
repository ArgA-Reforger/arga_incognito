//! Bridges voice traffic to the incognito system. The engine fires OnVoNUsed on the server on the
//! speaker's ACTIVE VON component, so `this` identifies the speaking volume; the static invoker
//! SCR_VoNComponent.GetOnVoNUsed() carries only a playerId and would lose it.
modded class SCR_VoNComponent
{
	//------------------------------------------------------------------------------------------------
	override protected event void OnVoNUsed(int senderId)
	{
		super.OnVoNUsed(senderId);

		ARGA_IncognitoComponent.NotifyVoiceUsed(senderId, this);
	}
}
