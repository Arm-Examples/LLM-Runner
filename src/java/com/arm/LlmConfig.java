//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm;

import java.util.List;

public class LlmConfig
{
    private String modelTag;
    private String userTag;
    private String modelPath;
    private String llmPrefix;
    private List<String> stopWords;
    private int numThreads;
    // minimal constructor without userTag and numThreads
    public LlmConfig(String modelTag, List<String> stopWords, String modelPath, String llmPrefix)
    {
        this(modelTag, stopWords, modelPath, llmPrefix, "", 4);
    }
    // minimal constructor without numThreads
    public LlmConfig(String modelTag, List<String> stopWords, String modelPath, String llmPrefix, String userTag)
    {
      // Use 4 threads by default
      this(modelTag, stopWords, modelPath, llmPrefix, userTag, 4);
    }
    // minimal constructor without userTag
    public LlmConfig(String modelTag, List<String> stopWords, String modelPath, String llmPrefix,int numThreads)
    {
       this(modelTag, stopWords, modelPath, llmPrefix, "", numThreads);
    }
    // main constructor
    public LlmConfig(String modelTag, List<String> stopWords, String modelPath,
                     String llmPrefix, String userTag, int numThreads)
    {
          this.modelTag = modelTag;
          this.stopWords = stopWords;
          this.modelPath = modelPath;
          this.llmPrefix = llmPrefix;
          this.userTag = userTag;
          this.numThreads = numThreads;
    }

    /**
     * Gets the model tag.
     *
     * @return The model tag.
     */
    public String getModelTag()
    {
        return this.modelTag;
    }
    /**
     * Gets the user tag.
     *
     * @return The user tag.
     */
    public String getUserTag()
    {
        return this.userTag;
    }
    /**
     * Gets the list of stop words.
     *
     * @return The list of stop words.
     */
    public List<String> getStopWords()
    {
        return this.stopWords;
    }

    /**
     * Gets the model path.
     *
     * @return The model path.
     */
    public String getModelPath()
    {
        return this.modelPath;
    }

    /**
     * Gets the LLM prefix.
     *
     * @return The LLM prefix.
     */
    public String getLlmPrefix()
    {
        return this.llmPrefix;
    }

    /**
    * Gets the number of Threads used
    * @return The number of Threads LLM uses.
    */
    public int getNumThreads()
    {
        return this.numThreads;
    }

    /**
     * Sets the model tag.
     *
     * @param modelTag The model tag to set.
     */
    public void setModelTag(String modelTag)
    {
        this.modelTag = modelTag;
    }

     /**
     * Sets the user tag.
     *
     * @param userTag The user tag to set.
     */
    public void setUserTag(String userTag)
    {
        this.userTag = userTag;
    }

    /**
     * Sets the list of stop words.
     *
     * @param stopWords The list of stop words to set.
     */
    public void setStopWords(List<String> stopWords)
    {
        this.stopWords = stopWords;
    }

    /**
     * Sets the model path.
     *
     * @param modelPath The model path to set.
     */
    public void setModelPath(String modelPath)
    {
        this.modelPath = modelPath;
    }

    /**
     * Sets the LLM prefix.
     *
     * @param llmPrefix The LLM prefix to set.
     */
    public void setLlmPrefix(String llmPrefix)
    {
        this.llmPrefix = llmPrefix;
    }

    /**
    * Sets the number of Threads.
    * @param numThreads count of threads to use for LLM.
    */
    public void setNumThreads(int numThreads)
    {
        this.numThreads = numThreads;
    }
}

