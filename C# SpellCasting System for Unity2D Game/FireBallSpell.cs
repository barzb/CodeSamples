using UnityEngine;
using System.Collections;

/*
    THIS SPELL CASTS A FIREBALL IN THE HAND OF THE CASTER
    THAT CAN BE FIRED AT ANY DIRECTION
*/
[RequireComponent (typeof(CircleCollider2D))]
[RequireComponent (typeof(SpriteRenderer))]
public class FireBallSpell : ISpell {

	public override float spellDuration     { get { return 5f; } }
	public override float finalizeDuration  { get { return 1f; } }
    
    // REFERENCES
    private Transform        handTransform;
	private CircleCollider2D ballCollider;
	private SpriteRenderer   fireBallSprite;
	private ParticleSystem   sparks;
	private ParticleSystem   fireBallExplode;
    private Rigidbody2D      rigid;

    // ATTRIBUTES
	public float speed = 50f;
	private bool isFired;
    private bool hasFired;
    private bool canFire;
	private bool explode;


	void Start()
	{
        // FIND COMPONENTS
		ballCollider = GetComponent<CircleCollider2D> ();
		fireBallSprite = GetComponent<SpriteRenderer> ();
		foreach (ParticleSystem p in GetComponentsInChildren<ParticleSystem> (false)) {
			if (p.name == "FireBallExplode")
				fireBallExplode = p;
			else if(p.name == "Flare")
				sparks = p;
		}
		if (fireBallExplode == null)
			Debug.LogError ("NO PARTICLE SYSTEM FOUND IN FIREBALL: FIREBALLEXPLODE");

        if (_caster == null)
            _caster = this.gameObject;
        handTransform = _caster.transform.Find("IK").GetChild(4);
        rigid = GetComponent<Rigidbody2D>();


        isFired = false;
        canFire = false;        
	}

	protected override void SpellFunction()
	{
        // FIREBALL IS NOW IN YOUR HAND
        if (!isFired)
        {
            // CHECK IF IT CAN BE FIRED

            // get left stick input
            float LX = Input.GetAxis("Horizontal");
            float LY = Input.GetAxis("Vertical");

            Vector2 targetDirection = new Vector2(LX, LY) / 2f;
            Vector2 direction = Vector2.zero;
            direction = targetDirection;

            // stick has to be in idle first 
            if ((Mathf.Abs(LX) < 0.3f || Mathf.Abs(LY) < 0.3f))
            {
                Invoke("SetCanFire", 1f);
            }

            // stick has moved out of idle
            if (Mathf.Abs(targetDirection.sqrMagnitude) > 0.7f)
			{
                // FIRE
				isFired = true;
				Vector3 dir = _targetPosition - transform.position;
				float angle = Mathf.Atan2(dir.y, dir.x) * Mathf.Rad2Deg - 90f;
				transform.rotation = Quaternion.AngleAxis(angle, Vector3.forward);
			}
        }

        // FIREBALL IS BEEING FIRED
        if (isFired)
        {
            // BUT HASN'T LEFT THE HAND YET
            if (!hasFired)
            {
                // LEAVE HAND -> ADD FORCE
                hasFired = true;
                rigid.isKinematic = (false);
                rigid.AddForce(direction.normalized * speed * 3f, ForceMode2D.Impulse);
            }
        }
        // FIREBALL ALWAYS IN HAND WHEN NOT FIRED
        else
        {
            transform.position = handTransform.position;
        }
    }
    
    // INVOKED AFTER 1 SEC DELAY
    // PLAYER CAN NOW FIRE THE FIREBALL
    private void SetCanFire()
    {
        canFire = true;
        hasFired = false;
    }

	protected override void FinalizeFunction()
    {
        // FIREBALL VANISHES WHEN NOT FIRED IN TIME (see: spellDuration)
		if (!isFired) {
			transform.position = _caster.transform.position + Vector3.up * 3f;
			fireBallSprite.transform.localScale = Vector3.Lerp (fireBallSprite.transform.localScale, new Vector3 (0, 0, 0), Time.fixedDeltaTime*5f);
			fireBallSprite.color = Color.Lerp (fireBallSprite.color, new Color (1f, 1f, 1f, 0f), Time.fixedDeltaTime * 5f);
			sparks.gameObject.SetActive (false);
		}
	}

    // EXPLODE ON COLLISION
    void OnCollisionEnter2D(Collision2D c)
    {
        // get spell target
        GameObject g = c.gameObject;
        if(g.CompareTag("SpellTarget"))
        {
            g.GetComponent<ISpellTarget>().TakeSpell(SpellType.FIRE);
        }
        // force the spell to finish, even if spellTime is still not zero (FinalizeFunction will be called from now on)
        Finalize();

        // play animation
		fireBallExplode.Play ();
        // play sound
		if(fireBallSprite.enabled)
			fireBallExplode.GetComponent<AudioSource> ().Play ();

        // deactivate fireball and spark emitter
        sparks.gameObject.SetActive(false);
        fireBallSprite.enabled = false;
    }
}
