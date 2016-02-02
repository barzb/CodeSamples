using UnityEngine;
using System.Collections;
using System.Collections.Generic;

// ENUM FOR SPElLCASTER CLASS, WILL BE TRANSLATED TO SPELL CLASSES
public enum SpellType 
{ 
	FIRE,
	RAIN,
	WIND,
    NULL
};

// INTERFACE FOR SPELLS
public abstract class ISpell : MonoBehaviour 
{
	// ATTRIBUTES
	private bool _isActivated;
	private float _activationTime;
	private bool _finalize;
	protected Vector3 _targetPosition;
	protected GameObject _caster;

	// REFERENCES
	private List<ISpell> _activeSpells;

	// PROPERTIES
	public bool isActivated { get { return _isActivated; } }
    // OVERRIDE THESE IN DERIVED SPELL CLASSES
	public abstract float spellDuration { get; }
	public abstract float finalizeDuration { get; }


	// UPDATE
	void FixedUpdate() {
		if (_isActivated) {
			if (!_finalize) {
				// SPELL IS ACTIVE
				if (Time.time < _activationTime + spellDuration) {
					SpellFunction ();
				}
				// SWITCH TO FINALIZE
				else {
					Finalize ();
				}
			} else {
				// SPELL IS FINALIZING
				if (Time.time < _activationTime + finalizeDuration) {
					FinalizeFunction ();
				}
				// SPELL IS FINALIZED -> DELETE
				else {
					_activeSpells.Remove (this);
					_isActivated = false;
					Destroy (this.gameObject);
				}
			}
		}
	}

    // FORCE THE SPELL TO USE FinalizeFunction INSTEAD OF SpellFunction
    // EVEN IF spellDuration IS STILL RUNNING
	protected void Finalize ()
	{
		_activationTime = Time.fixedTime;
		_finalize = true;
	}


    // INIT SPELL
	public void Activate(List<ISpell> activeSpells, Vector3 targetPosition, GameObject caster)
	{
        // GLOBAL SPELL POSITION, CAN BE OVERWRITTEN BY Start() FUNCTION IN DERIVED SPELL CLASSES
		transform.position = new Vector3(0,0,0);

        // SET ATTRIBUTES
		_caster = caster;
		_targetPosition = targetPosition;
		_activeSpells = activeSpells;
		_activationTime = Time.fixedTime;
		_isActivated = true;
		_finalize = false;

        // ADD TO SPELL LIST
		_activeSpells.Add (this);
	}

	// OVERRIDE IN DERIVED SPELL CLASSES
	protected abstract void SpellFunction();
	protected abstract void FinalizeFunction();
}
