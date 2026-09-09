/******************************************************************************
 * Spine Runtimes Software License
 * Version 2.3
 *
 * Copyright (c) 2013-2015, Esoteric Software
 * All rights reserved.
 *
 * You are granted a perpetual, non-exclusive, non-sublicensable and
 * non-transferable license to use, install, execute and perform the Spine
 * Runtimes Software (the "Software") and derivative works solely for personal
 * or internal use. Without the written permission of Esoteric Software (see
 * Section 2 of the Spine Software License Agreement), you may not (a) modify,
 * translate, adapt or otherwise create derivative works, improvements of the
 * Software or develop new applications using the Software or (b) remove,
 * delete, alter or obscure any trademarks or any copyright, trademark, patent
 * or other intellectual property or proprietary rights notices on or in the
 * Software, including any copy thereof. Redistributions in binary or source
 * form must include this license and terms.
 *
 * THIS SOFTWARE IS PROVIDED BY ESOTERIC SOFTWARE "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL ESOTERIC SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include <spine_3_4/SkeletonBatch.h>
#include <spine_3_4/extension.h>
#include <algorithm>

USING_NS_AX;

#define EVENT_AFTER_DRAW_RESET_POSITION "director_after_draw"
#define INITIAL_SIZE (64)

namespace spine34
{

static SkeletonBatch* instance = nullptr;

SkeletonBatch* SkeletonBatch::getInstance()
{
    if (!instance)
        instance = new SkeletonBatch();
    return instance;
}

void SkeletonBatch::destroyInstance()
{
    if (instance)
    {
        delete instance;
        instance = nullptr;
    }
}

SkeletonBatch::SkeletonBatch()
{
    auto program  = backend::Program::getBuiltinProgram(backend::ProgramType::POSITION_TEXTURE_COLOR);
    _programState = new backend::ProgramState(program);
    for (unsigned int i = 0; i < INITIAL_SIZE; i++)
        _commandsPool.push_back(newCommand());
    reset();
    Director::getInstance()->getEventDispatcher()->addCustomEventListener(
        EVENT_AFTER_DRAW_RESET_POSITION, [this](EventCustom* /*eventCustom*/) { this->update(0); });
}

SkeletonBatch::~SkeletonBatch()
{
    Director::getInstance()->getEventDispatcher()->removeCustomEventListeners(EVENT_AFTER_DRAW_RESET_POSITION);
    for (unsigned int i = 0; i < _commandsPool.size(); i++)
    {
        AX_SAFE_RELEASE(_commandsPool[i]->getPipelineDescriptor().programState);
        delete _commandsPool[i];
        _commandsPool[i] = nullptr;
    }
    AX_SAFE_RELEASE(_programState);
}

backend::ProgramState* SkeletonBatch::updateCommandPipelinePS(SkeletonCommand* command,
                                                              backend::ProgramState* programState)
{
    auto& currentState = command->getPipelineDescriptor().programState;
    if (currentState == nullptr || currentState->getBatchId() != programState->getBatchId())
    {
        AX_SAFE_RELEASE(currentState);
        currentState             = programState->clone();
        command->_locMVP         = currentState->getUniformLocation(backend::UNIFORM_NAME_MVP_MATRIX);
        command->_locTexture     = currentState->getUniformLocation(backend::UNIFORM_NAME_TEXTURE);
    }
    return currentState;
}

void SkeletonBatch::update(float /*delta*/)
{
    reset();
}

ax::TrianglesCommand* SkeletonBatch::addCommand(ax::Renderer* renderer, float globalOrder, ax::Texture2D* texture,
                                                ax::backend::ProgramState* programState, ax::BlendFunc blendType,
                                                const ax::TrianglesCommand::Triangles& triangles, const ax::Mat4& mv,
                                                uint32_t flags)
{
    SkeletonCommand* command                  = nextFreeCommand();
    const ax::Mat4& projectionMat             = Director::getInstance()->getMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION);

    if (programState == nullptr)
        programState = _programState;

    AXASSERT(programState, "programState should not be null");

    auto pipelinePS = updateCommandPipelinePS(command, programState);
    pipelinePS->setUniform(command->_locMVP, projectionMat.m, sizeof(projectionMat.m));
    pipelinePS->setTexture(command->_locTexture, 0, texture->getBackendTexture());

    command->init(globalOrder, texture, blendType, triangles, mv, flags);
    renderer->addCommand(command);
    return command;
}

void SkeletonBatch::reset()
{
    _nextFreeCommand = 0;
}

SkeletonCommand* SkeletonBatch::nextFreeCommand()
{
    if (_commandsPool.size() <= _nextFreeCommand)
    {
        unsigned int newSize = (unsigned int)_commandsPool.size() * 2 + 1;
        for (unsigned int i = (unsigned int)_commandsPool.size(); i < newSize; i++)
            _commandsPool.push_back(newCommand());
    }
    return _commandsPool[_nextFreeCommand++];
}

SkeletonCommand* SkeletonBatch::newCommand()
{
    return new SkeletonCommand();
}

}  // namespace spine34
