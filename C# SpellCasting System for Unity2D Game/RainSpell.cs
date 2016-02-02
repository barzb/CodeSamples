using UnityEngine;
using System.Collections;

/*
    THIS SPELL CASTS A CLOUD WHICH STARTS TO RAIN DOWN
*/
[RequireComponent(typeof(AudioSource))]
public class RainSpell : ISpell {

	public override float spellDuration     { get { return 10f; } }
	public override float finalizeDuration  { get { return 5f; } }

	private ParticleSystem rain;
	private Animator cloud;

	void Start()
	{
        // FIND COMPONENTS
		rain = GetComponentInChildren<ParticleSystem> (false);
		cloud = GetComponentInChildren<Animator> (false);
        // SET ANIMATION TRIGGER
		cloud.SetTrigger ("CreateCloud");

		if (_caster == null)
			_caster = this.gameObject;

        // CREATE CLOUD 5 UNITS LEFT OR RIGHT FROM PLAYER, DEPENDING ON HIS FACING DIRECTION
		float characterLookDirection = (_caster.GetComponent<CharacterMovement> ().isTurnedLeft) ?
			-5f : +5f;
		Vector3 pos = transform.position;
		pos.x = _caster.transform.position.x + characterLookDirection;
		pos.y = Camera.main.transform.position.y + Camera.main.orthographicSize/2f - 1.2f;
		transform.position = pos;

        // PLAY AUDIO
		GetComponent<AudioSource> ().Stop ();
		GetComponent<AudioSource> ().PlayDelayed (2f);
	}

	protected override void SpellFunction()
	{
        // EVERYTHING IS DONE BY ANIMATOR AND AUTO-PLAY PARTICLE SYSTEM
	}

	protected override void FinalizeFunction()
	{
        // STOP THE PARTICLE SYSTEM
		rain.Stop ();
        // SET DESTROY CLOUD ANIMATION TRIGGER
		cloud.SetTrigger ("DestroyCloud");
	}


}
