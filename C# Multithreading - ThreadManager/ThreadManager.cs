using UnityEngine;
using System.Collections;
using System.Collections.Generic;

// SINGLETON CLASS FOR THREAD MANAGEMENT
public sealed class ThreadManager
{
    // singleton
    private static ThreadManager instance;

    // list of active working threads
    private static List<WorkingThread> threads;
    private static int maxThreads;
    private static int activeThreads;

    // PROPERTIES
    public int ActiveThreads { get { return activeThreads; } }

    // only way to create a thread manager. there can only be one!
    public static ThreadManager Instance(int _maxThreads)
    {
        if (_maxThreads < 0)
            _maxThreads = 0;

        if(instance == null) {
            instance = new ThreadManager(_maxThreads);
        } else {
            maxThreads = _maxThreads;
        }

        return instance;        
    }

    // private constructor
	private ThreadManager(int _maxThreads)
    {
        maxThreads = _maxThreads;
        threads = new List<WorkingThread>();
        activeThreads = 0;
    }


    // abort all active threads
    public void AbortThreads()
    {
        foreach (WorkingThread t in threads) {
            t.Abort();
        }
    }

    // add a working thread to the list of active threads
    private WorkingThread AddThread()
    {
        WorkingThread thread = new WorkingThread();
        threads.Add(thread);
        thread.Initialize();

        activeThreads = threads.Count;

        return thread;
    }

    // look for empty/dead threads and delete them from the list of active threads
    public void CleanUp()
    {
        List<WorkingThread> deadThreads = new List<WorkingThread>();

        // look for dead threads
        foreach (WorkingThread t in threads)
        {
            if (!t.isAlive)
            {
                Debug.Log("DEAD THREAD DETECTED... DELETE");
                deadThreads.Add(t);
            }
        }

        // delete dead threads
        foreach (WorkingThread t in deadThreads)
            threads.Remove(t);
        
        activeThreads = threads.Count;
    }

    // return the least busy thread 
    public WorkingThread GrabThread()
    {
        // remove dead threads from list
        CleanUp();

        // create new thread if there is space for it
        if (threads.Count < maxThreads) {
            return AddThread();
        }

        // check for the least busy thread
        WorkingThread laziestThread = null;

        foreach (WorkingThread t in threads)
        {
            if (laziestThread == null) { 
                laziestThread = t;
            } else if (t.JobsLeft < laziestThread.JobsLeft) { 
                laziestThread = t;
            }
        }

        return laziestThread;
    }
}
