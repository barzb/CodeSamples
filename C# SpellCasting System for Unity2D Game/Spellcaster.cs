using UnityEngine;
using System.Collections;
using System.Collections.Generic;

// ASSIGN THIS TO EVERY PLAYER/ENEMY WHO CAN CAST SPELLS
public class Spellcaster : MonoBehaviour {

    // List of active, instantiated spells
	private List<ISpell> _spellList;

    // Prefabs
	public FireBallSpell fireBallSpellPrefab;
	public RainSpell 	 rainSpellPrefab;
    public WindSpell     windSpellPrefab;

    // Sound for successful cast
	public AudioClip successCastSound;

	// INIT
	void Start () {
		_spellList = new List<ISpell> ();
		if (successCastSound == null)
			Debug.Log ("No sound selected for successful spellcast :(");
	}
	
    // UPDATE
	void Update () {
        // FOR DEBUG ONLY, SPELLS WILL BE CASTED AFTER SUCCESSFUL DANCE COMBINATION
        if (Input.GetKeyDown(KeyCode.Alpha1))       // RAIN
            castSpell (SpellType.RAIN, Camera.main.ScreenToWorldPoint(Input.mousePosition));
		else if (Input.GetKeyDown(KeyCode.Alpha2))  // FIREBALL
			castSpell (SpellType.FIRE, Camera.main.ScreenToWorldPoint(Input.mousePosition));
        else if (Input.GetKeyDown(KeyCode.Alpha3))  // WIND
            castSpell (SpellType.WIND, Camera.main.ScreenToWorldPoint(Input.mousePosition));
    }


    // CAN BE CALLED FROM EVERYWHERE
	public void castSpell(SpellType type, Vector3 targetLocation)
	{
		ISpell spell = null;

        // INSTANTIATE SPELL FROM TYPE
		switch (type) {
		case SpellType.FIRE:
            if(fireBallSpellPrefab != null)
                spell = Instantiate (fireBallSpellPrefab) as ISpell;
			break;

		case SpellType.RAIN:
            if(rainSpellPrefab != null)
                spell = Instantiate (rainSpellPrefab) as ISpell;
			break;

        case SpellType.WIND:
            if (windSpellPrefab != null)
                spell = Instantiate (windSpellPrefab) as ISpell;
             break;

		default:
			Debug.LogError ("Invalid Spell Type");
			return;
		}

        // INIT SPELL
		if (spell != null) {
			if (successCastSound != null) 
				AudioSource.PlayClipAtPoint (successCastSound, Camera.main.transform.position);
			spell.Activate (_spellList, targetLocation, this.gameObject);
		}
	}
}
